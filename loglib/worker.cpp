#include "worker.h"
#include "manager.h"

#include <iostream>
#include <string>
#include <assert.h>
#include <chrono>

#include "3rdparty/fmtlib/format.h"
#include "3rdparty/fmtlib/chrono.h"

#include "3rdparty/date.h"
#include "3rdparty/httplib.h"
#include "3rdparty/json.hpp"

namespace Logger
{

Worker::Worker(const std::string& token, const std::string& host, uint16_t port, size_t packet_size, size_t flush_buffer_size, bool concat_records) :
	_token(token), _host(host), _port(port), _packet_size(packet_size), _flush_buffer_size(flush_buffer_size), _concat_records(concat_records)
{
	assert(!_host.empty());
	assert(_port > 0);
	assert(_packet_size > 0);
}

bool Worker::SendToServer(const std::vector<RecordPtr>& records, int& error_code, std::string& error_string)
{		
	error_code = 0;
	error_string.clear();

	using json = nlohmann::json;

	auto j_total = json::array();

	for (auto& r : records)
	{
		auto j_obj = json::object();

		j_obj["logTime"] = date::format("%FT%TZ", date::floor<std::chrono::microseconds>(r->time));
		j_obj["service"] = r->service;
		j_obj["source"] = r->source;
		j_obj["category"] = r->category;
		j_obj["level"] = r->level;
		j_obj["session"] = r->session;
		j_obj["info"] = r->info;
		j_obj["url"] = r->url;
		j_obj["httpType"] = r->httpType;
		j_obj["httpCode"] = r->httpCode;
		j_obj["errorCode"] = r->errorCode;
		j_obj["properties"] = r->properties;
		j_obj["httpHeaders"] = r->httpHeaders;

		if (!r->jsonBody.empty())
		{
			try
			{
				j_obj["body"] = json::parse(r->jsonBody);
			}
			catch (...)
			{
			}
		}

		j_total.push_back(j_obj);
	}

	httplib::Client cli(_host, _port);
	//	cli.set_connection_timeout(2);
	//	cli.set_read_timeout(5, 0);
	//	cli.set_write_timeout(5, 0);

	try
	{
		auto res = cli.Post("/api/add",
							{
								{"X-Authorization", _token},
								{"Connection", "keep-alive"},
								{"Content-Type", "application/json"},
								{"User-Agent", "loglib"},
							},
							j_total.dump(),
							"application/json");
		if (!res)
		{
			error_code = (int)res.error();
			error_string = to_string(res.error());
			return false;
		}

		if (res->status != 201)
		{
			error_code = res->status;
			error_string = res->reason + ", " + res->body;
			return false;
		}
	}
	catch (...)
	{
		// кривые данные? игнорируем
		error_code = 400;
		error_string = "invalid data";
		return false;
	}
	return true;
}

void Worker::Start(size_t number)
{
	_number = number;
//	Manager::CoutPrint(fmt::format("worker {} started", _number), false);

	while (!IsStopRequested())
	{
		while (true)
		{
			size_t process_size = _packet_size;
			bool full_lock = false;

			if (_flush_buffer_size > 0)
			{
				_buffer_mutex.lock();
				size_t size = _buffer.size();
				_buffer_mutex.unlock();
				if (size > _flush_buffer_size)
				{
					process_size = size;
					full_lock = true;
					Manager::CoutPrint(fmt::format("Worker {} buffer full => auto flush", _number), true);
				}
			}

			if (!ProcessBuffer(process_size, full_lock))
				break;
		}

		// спим какое-то время или пробуждаемся при вызове AddRecord
		std::unique_lock<std::mutex> lock(_wakeup_mutex);
		_wakeup.wait_for(lock, std::chrono::seconds(1));
	}

	Flush();

//	Manager::CoutPrint(fmt::format("worker {} finished", _number), false);
}

void Worker::Flush()
{
	ProcessBuffer(BufferSize(), true);
}

void Worker::AddRecord(const RecordPtr& record)
{
	_buffer_mutex.lock();
	_buffer.push(record);
	_buffer_mutex.unlock();

	// будим обработчик если он решил поспать
	_wakeup.notify_one();
}

size_t Worker::BufferSize() const
{
	std::lock_guard<std::mutex> lock(_buffer_mutex);
	return _buffer.size();
}

void Worker::StopRequest()
{
//	Manager::CoutPrint(fmt::format("worker {} finishing...", _number), false);

	StoppableWorker::StopRequest();

	// будим обработчик если он решил поспать
	_wakeup.notify_one();
}

bool Worker::ProcessRecords(const std::vector<RecordPtr>& records, int& error_code, std::string& error_text)
{
	if (_concat_records)
		return SendToServer(records, error_code, error_text);

	for (auto i = records.begin(); i != records.end(); ++i)
	{
		if (!SendToServer({*i}, error_code, error_text))
			return false;
	}
	return true;
}

void Worker::ProcessErrorRecords(const std::vector<RecordPtr>& records, int error_code, const std::string& error_text)
{
	Manager::SaveErrors(records, error_code, error_text);
}

bool Worker::ProcessBuffer(size_t packet_size, bool full_lock)
{
	std::vector<RecordPtr> records;

	_buffer_mutex.lock();
	while (!_buffer.empty() && records.size() < packet_size)
	{
		records.push_back(_buffer.front());
		_buffer.pop();
	}

	if (!full_lock)
		_buffer_mutex.unlock();

	// обрабатываем записи
	if (!records.empty())
	{
		int error_code;
		std::string error_text;
		if (!ProcessRecords(records, error_code, error_text))
			ProcessErrorRecords(records, error_code, error_text);
		else
			Manager::RegisterProcessedCount(records.size());
	}

	if (full_lock)
		_buffer_mutex.unlock();

	return !records.empty();
}

} // namespace Logger
