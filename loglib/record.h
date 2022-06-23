#pragma once

#include <memory>
#include <chrono>
#include <ctime>
#include <string>
#include <map>

namespace Logger
{

struct Record
{
	Record() : time(std::chrono::system_clock::now()) {}

	std::chrono::time_point<std::chrono::system_clock>  time;
	std::string service;
	std::string source;
	std::string category;
	std::string level;
	std::string session;
	std::string info;
	std::string url;
	std::string httpType;
	int httpCode = 0;
	int errorCode = 0;
	std::string jsonBody;

	std::map<std::string, std::string> properties;
	std::map<std::string, std::string> httpHeaders;
};

using RecordPtr = std::shared_ptr<Record>;

} // namespace Logger
