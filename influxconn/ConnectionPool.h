#ifndef CONNECTION_POOL_H__ // #include guards
#define CONNECTION_POOL_H__

#include <vector>
#include <map>
#include <chrono>
#include <unordered_set>

#include "InfluxConnection.h"
#include "concurrentqueue.h"

class ConnectionPool
{
public:
    ConnectionPool(
        std::string server, int port, std::string user, 
        std::string password, std::string org, std::string bucket, 
        std::string database, int numConnection);

    ~ConnectionPool();

    InfluxConnection* GetConnecion(unsigned int timeout=0);
    bool ReleaseConnecion(InfluxConnection* sqlPtr);

    bool OpenPoolConnections();
    void ResetPoolConnections();
    void ClosePoolConnections();

    bool HasActiveConnections();

private:
    std::atomic_flag _pool_mutex;
    bool hasActiveConnections;
    std::unordered_set<int> Indexes;
    moodycamel::ConcurrentQueue<int> connectionQueue;
    std::vector<std::unique_ptr<InfluxConnection>> influxSqlPtrList;
};


ConnectionPool::ConnectionPool(
    std::string server, int port, std::string user, std::string password, 
    std::string org, std::string bucket, std::string database, int numConnection)
{
    if(server.length() == 0 || user.length() == 0)
        std::cerr << "Server or user name is empty or NULL." << std::endl;
    else
    {
        std::cout << "Creating connection pool server=" 
            << server << " database=" << database << std::endl;

        hasActiveConnections = false;
        bool success = false;
        try
        {
            _pool_mutex.test_and_set(std::memory_order_acquire);
            for(int i=0; i < numConnection; i++)
            {
                influxSqlPtrList.emplace_back(
                    new InfluxConnection(server, port, user, password, database, org, bucket, i));

                success = false;
                success = influxSqlPtrList[i]->open();
                
                if(success)
                {
                    connectionQueue.enqueue(i);
                    Indexes.insert(i);
                }
                else
                {
                    std::cerr << "Connection pool failed. Cannot connect to server." << std::endl;
                    ClosePoolConnections();
                    break;
                }
            }

            size_t count = influxSqlPtrList.size();
            if(success && count > 0 && count == connectionQueue.size_approx())
            {
                hasActiveConnections = true;
                std::cout << "Pool created successfully." << std::endl;
            }
            _pool_mutex.clear(std::memory_order_release);
        }
        catch(const std::exception& e)
        {
            ClosePoolConnections();
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
}

ConnectionPool::~ConnectionPool()
{
    //ClosePoolConnections();
}

bool ConnectionPool::HasActiveConnections()
{
    return hasActiveConnections;
}


InfluxConnection* ConnectionPool::GetConnecion(unsigned int timeout)
{
    if(!hasActiveConnections)
    {
        std::cerr << "No active sql connection." << std::endl;
        return nullptr;
    }

    if(timeout < 0)
        std::cerr << "Error: Get connection Timeout value less then 0." << std::endl;

    int ind;
    bool success = false;
    auto begin = std::chrono::system_clock::now();

    do
    {
        success = connectionQueue.try_dequeue(ind);
        if(success && ind < influxSqlPtrList.size())
        {
            _pool_mutex.test_and_set(std::memory_order_acquire);
            auto it = Indexes.find(ind);
            if(it != Indexes.end())
                Indexes.erase(ind);
            _pool_mutex.clear(std::memory_order_release);
            return influxSqlPtrList[ind].get();
        }

        // set max waiting time to get connection
        // return nullptr on time out
        if(timeout > 0)
        {
            auto end = std::chrono::system_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(end - begin).count();
            if(elapsed >= timeout)
                return nullptr;
        }

    } while (!success);
    
    return nullptr;
}


bool ConnectionPool::ReleaseConnecion(InfluxConnection* sqlPtr)
{
    if(sqlPtr->getPoolId() > -1)
    {
        _pool_mutex.test_and_set(std::memory_order_acquire);
        auto it = Indexes.find(sqlPtr->getPoolId());
        if(it == Indexes.end())
        {
            connectionQueue.enqueue(sqlPtr->getPoolId());
            Indexes.insert(sqlPtr->getPoolId());
        }
        _pool_mutex.clear(std::memory_order_release);
        return true;
    }
    return false;
}

bool ConnectionPool::OpenPoolConnections()
{
    ClosePoolConnections();
}

void ConnectionPool::ClosePoolConnections()
{
    hasActiveConnections = false;
    for(auto& sqlPtr: influxSqlPtrList)
    {
        if(sqlPtr != nullptr && sqlPtr->isValide())
            sqlPtr->disconnect();
    }

    _pool_mutex.test_and_set(std::memory_order_acquire);
    connectionQueue = moodycamel::ConcurrentQueue<int>();
    Indexes = std::unordered_set<int>();
    _pool_mutex.clear(std::memory_order_release);
}

void ConnectionPool::ResetPoolConnections()
{
    bool success = false;
    ClosePoolConnections();
    for(auto& sqlPtr: influxSqlPtrList)
    {
        success = sqlPtr->open();
        if(success)
        {
            _pool_mutex.test_and_set(std::memory_order_acquire);
            Indexes.insert(sqlPtr->getPoolId());
            connectionQueue.enqueue(sqlPtr->getPoolId());
            _pool_mutex.clear(std::memory_order_release);
        }
        else
        {
            std::cerr << "Connection pool failed. Cannot connect to server." << std::endl;
            for(auto& sqlPtr : influxSqlPtrList)
            {
                if(sqlPtr != nullptr && sqlPtr->isValide())
                    sqlPtr->disconnect();
            }
        }
    }

    size_t count = influxSqlPtrList.size();
    if(success && count > 0 && count == connectionQueue.size_approx())
        hasActiveConnections = true;
}

#endif
