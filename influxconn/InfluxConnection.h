#ifndef INFLUX_CONNECTION_H__ // #include guards
#define INFLUX_CONNECTION_H__

#include <string>
#include <iostream>
#include <thread>      
#include <chrono> 
#include <vector>
#include "influxdb.hpp"

using namespace std;
using namespace influxdb_cpp;

class InfluxConnection
{
public:
	InfluxConnection(string server, int port, string user, string token, string database); 
	InfluxConnection(string server, int port, string user, string token,
					string database, string org, string bucket, int id); 
	~InfluxConnection();
	
	bool open(int retry=2);
	bool disconnect();
	bool isValide();

	std::string getServer();
	std::string getDatabase();
	std::string getUser();
	int getSocketId();
	int getPoolId();

	std::shared_ptr<server_info> siPtr; 

private:
	int index;
	int sock;
	int port;
	string server, user, token, database;
	struct sockaddr_in addr;
};


///constructor to set mysql connection variables
InfluxConnection::InfluxConnection(
	std::string server, int port, string user, string token, std::string database) 
{
	this->server = server;
	this->port = port;
	this->user = user;
	this->token = token;
	this->database = database;
	this->index = -1;
	this->sock = -1;
	this->siPtr.reset(new server_info(server, port, database, token));
}

InfluxConnection::InfluxConnection(
	std::string server, int port, string user, string token,
	string database, string org, string bucket, int id) 
{
	this->server = server;
	this->port = port;
	this->user = user;
	this->token = token;
	this->database = database;
	this->index = id;
	this->sock = -1;
	this->siPtr.reset(new server_info(server, port, database, token, org, bucket));
}

InfluxConnection::~InfluxConnection()
{
	this->disconnect();
}

bool InfluxConnection::open(int retry)
{
	bool success = false;
	if(retry <= 0 )
	{
		std::cout << "Failed to connect to host=" << server 
				<< " db=" << database << std::endl;
		return false;
	}
	
	try
	{
		addr.sin_family = AF_INET;
		addr.sin_port = htons(siPtr->port_);
		if((addr.sin_addr.s_addr = inet_addr(siPtr->host_.c_str())) == INADDR_NONE)
		{
			std::cout << "Error: could not bind to " << siPtr->host_ 
						<< " at port " << siPtr->port_ << std::endl;
			return success;
		}

		if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			std::cout << "Error creating SOCK_STREAM\n";
			return success;
		}

		if(connect(this->sock, (struct sockaddr*)(&addr), sizeof(addr)) < 0) 
		{
			std::cout << "Error socket connect\n";
			closesocket(this->sock);
			success = this->open(retry--);
		}

		success = true;
	}
	catch(const std::exception& e)
	{
		std::cerr << "ERROR Connectdb: " << e.what() << '\n';
		return success;
	}
	return success;
}

bool InfluxConnection::disconnect()
{
	closesocket(this->sock);
}

bool InfluxConnection::isValide()
{
	if (this->sock >= 0)
		return true;
	return false;
}


std::string InfluxConnection::getServer()
{
	return this->server;
}

std::string InfluxConnection::getDatabase()
{
	return this->database;
}
	
std::string InfluxConnection::getUser()
{
	return this->user;
}

int InfluxConnection::getSocketId()
{
	return this->sock;
}

int InfluxConnection::getPoolId()
{
	return this->index;
}

#endif
