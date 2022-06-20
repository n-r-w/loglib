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
	std::string service;
	std::string source;
	std::string category;
	std::string level;
	std::string session;
	std::string info;
	std::string url;
	std::string httpType;
	std::string jsonBody;

	std::map<std::string, std::string> properties;
	std::map<std::string, std::string> httpHeaders;
};

using RecordPtr = std::shared_ptr<Record>;

} // namespace Logger
