#include <locale>
#include <ctime>
#include <chrono>
#include <mutex>
#include <map>
#include <csignal>
#include <sstream>
#include <fstream>
#include <thread>
#include <random>
#include <iostream>
#include "../influxconn/ConnectionPool.h"


bool running = true;
std::shared_ptr<ConnectionPool> connPool;


std::string GetProgramDir(char** argv)
{
    std::string programPath(argv[0]);
    size_t pos = programPath.rfind('/');
    if (pos == programPath.npos)
    {
        std::cerr << "Could get program directory." << std::endl;
        exit(EXIT_FAILURE);
    }

    return programPath.substr(0, pos);
}


std::map<std::string, std::string> ReadConfigFile(std::string filename)
{
    std::map<std::string, std::string> params;

    std::ifstream stream(filename);
    if (stream.is_open())
    {
        std::string line;
        while (std::getline(stream, line))
        {
            if(line.length() == 0 || line.find('#') != line.npos)
                continue;
                
            std::stringstream ss(line);
            std::string name;
            std::string value;

            ss >> name;
            ss >> value;
            params[name] = value;
        }
        stream.close();
    }
    else
    {
        std::cerr << filename << " does not exists." << std::endl;
    }
    
    return params;
}


int main(int argc, char** argv)
{
	std::stringstream ssdbfile;

	// signal hadler for Ctrl + C to stop program
	std::signal(SIGINT, [](int signal){
		running = false;
		std::cout << "Program Interrupted." << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(2));
        exit(EXIT_SUCCESS);
	}); 

	std::cout << "Starting...\n";
	
	std::string programDir = GetProgramDir(argv);

    ssdbfile << programDir << "/config.txt";
    auto dbconfigs = ReadConfigFile(ssdbfile.str());

	auto hit = dbconfigs.find("dbhost");
	auto pit = dbconfigs.find("port");
	auto uit = dbconfigs.find("user");
	auto pwdit = dbconfigs.find("password");
	auto dit = dbconfigs.find("database");
    auto tbit = dbconfigs.find("table");

	if(pit == dbconfigs.end() || hit == dbconfigs.end() || uit == dbconfigs.end() || 
        pwdit == dbconfigs.end() || dit == dbconfigs.end() || tbit == dbconfigs.end())
	{
		std::cerr << "Could not find server/port in config!";
		exit(EXIT_FAILURE);
	}

    int port = std::stoi(dbconfigs.at("port"));
    std::string database = dbconfigs.at("database");
    std::string table = dbconfigs.at("table");

    size_t NUM_CONNS = 3;
    connPool.reset(new ConnectionPool(
                    dbconfigs.at("dbhost"), port, dbconfigs.at("user"), 
                    dbconfigs.at("password"), database, NUM_CONNS));

    if(!connPool->HasActiveConnections())
    {
        std::cerr << "Error Initializing connection pool!" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Connection Initialized!" << std::endl;

    std::uniform_real_distribution<double> unif(0, 1);

    while(running) // Crtl + C to stop
    {
        auto influxPtr = connPool->GetConnecion();
        std::thread([influxPtr, table]()
        {
            string resp;
            double value = (rand() % 10) + 1;
            int ret = influxdb_cpp::builder()
                .meas(table)
                .tag("host", "server_1")
                .tag("region", "eu_central")
                .field("value", value)
                .post_http(influxPtr->getSocketId(), *influxPtr->siPtr.get(), &resp);

            cout << "POST: " << ret << endl << resp << endl;

            connPool->ReleaseConnecion(influxPtr);
        }).detach();
    }

    return 0;
}

