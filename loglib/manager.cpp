#include "manager.h"
#include "worker.h"
#include <iostream>
#include <assert.h>
#include <regex>
#include <chrono>

#include "3rdparty/fmtlib/format.h"
#include "3rdparty/fmtlib/chrono.h"
#include "3rdparty/fmtlib/ranges.h"
#include "3rdparty/httplib.h"

namespace Logger
{
std::string Manager::_host;
uint16_t Manager::_port = 0;

std::mutex Manager::_manager_mutex;
std::shared_ptr<Manager> Manager::_manager;
std::unique_ptr<std::thread> Manager::_manager_thread;

std::mutex Manager::_cout_locker;

std::mutex Manager::_file_locker;
std::string Manager::_error_file_name;
std::ofstream Manager::_log_file;

std::atomic<int64_t> Manager::_processed_count = 0;
std::atomic<std::chrono::steady_clock::time_point> Manager::_processed_time;

void Manager::StartHelper(const std::string& token, const std::string& host, uint16_t port, size_t workers_count, size_t packet_size, size_t flush_buffer_size, bool concat_records)
{
	assert(workers_count > 0);

	for (size_t i = 0; i < workers_count; i++)
	{
		auto worker = std::make_shared<Worker>(token, host, port, packet_size, flush_buffer_size, concat_records);
		auto thread = std::make_unique<std::thread>([worker, i]() { worker->Start(i); });

		_workers.push_back(worker);
		_worker_threads.push_back(std::move(thread));
	}

	_started = true;

	while (!IsStopRequested())
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

void Manager::StopHelper()
{
	for (size_t i = 0; i < _workers.size(); i++)
	{
		_workers.at(i)->StopRequest();
	}

	for (size_t i = 0; i < _worker_threads.size(); i++)
	{
		_worker_threads.at(i)->join();
	}

	_worker_threads.clear();
	_workers.clear();

	StopRequest();
}

void Manager::AddRecordHelper(const RecordPtr& record)
{
	assert(record != nullptr);

	// выбираем поток с самой меньшей очередью
	WorkerPtr best_worker;
	for (size_t i = 0; i < _workers.size(); i++)
	{
		if (best_worker == nullptr || best_worker->BufferSize() > _workers.at(i)->BufferSize())
			best_worker = _workers.at(i);
	}

	best_worker->AddRecord(record);
}

size_t Manager::BufferSizeHelper() const
{
	size_t size = 0;
	for (size_t i = 0; i < _workers.size(); i++)
	{
		size += _workers.at(i)->BufferSize();
	}

	return size;
}

std::string Manager::Start(const std::string& token, const std::string& host, uint16_t port, size_t workers_count, size_t packet_size,
						   size_t flush_buffer_size, bool concat_records, const std::string& error_file_name)
{
	std::lock_guard<std::mutex> lock(_manager_mutex);

	assert(_manager == nullptr);
	assert(!error_file_name.empty());

	_host = host;
	_port = port;

	_processed_time = std::chrono::steady_clock::now();
	_error_file_name = error_file_name;
	_manager = std::make_shared<Manager>();
	_manager_thread = std::make_unique<std::thread>(
		[m = _manager, token, workers_count, packet_size, flush_buffer_size, host, port, concat_records]() {
			_manager->StartHelper(token, host, port, workers_count, packet_size, flush_buffer_size, concat_records);
		});

	return {};
}

void Manager::Stop()
{
	std::lock_guard<std::mutex> lock(_manager_mutex);

	if (_manager == nullptr || !_manager->_started)
		return;

	_manager->StopHelper();
	_manager_thread->join();

	_manager_thread.reset();
	_manager.reset();

	_log_file.close();
}

void Manager::AddRecord(const RecordPtr& record)
{
	std::lock_guard<std::mutex> lock(_manager_mutex);

	if (_manager == nullptr || !_manager->_started)
		return;

	_manager->AddRecordHelper(record);
}

size_t Manager::BufferSize()
{
	std::lock_guard<std::mutex> lock(_manager_mutex);
	return _manager && _manager->_started ? _manager->BufferSizeHelper() : 0;
}

void Manager::CoutPrint(const std::string& message)
{
	if (message.empty())
		return;

	std::lock_guard<std::mutex> lock(_cout_locker);
	std::cout << message << std::endl;
}

void Manager::SaveErrors(const std::vector<RecordPtr>& records, int error_code, const std::string& error_text)
{
	if (_error_file_name.empty())
		return;

	std::lock_guard<std::mutex> lock(_file_locker);
	if (!_log_file.is_open())
	{
		_log_file.exceptions(~std::ofstream::goodbit);
		try
		{
			_log_file.open(_error_file_name, std::fstream::app);
		}
		catch (std::ofstream::failure err)
		{
			CoutPrint(fmt::format("file output error: {}", err.what()));
			return;
		}
	}
	_log_file << fmt::format("error: {}, {}", error_code, error_text) << std::endl;
	for (auto& r : records)
	{
		_log_file << fmt::format("{:%Y-%m-%d %H:%M:%S}, "
								 "service: {}, source: {}, category: {}, level: {}, session: {}, info: {}, url: {}, httpType: {}, "
								 "properties: {}, httpHeaders: {}", std::chrono::system_clock::now(),
								 r->service, r->source, r->category, r->level, r->session, r->info, r->url, r->httpType, r->properties, r->httpHeaders) << std::endl;
		_log_file << "jsonBody: " << r->jsonBody << std::endl;
	}
}

void Manager::RegisterProcessedCount(uint64_t n)
{
	_processed_count += n;
}

uint64_t Manager::TotalProcessed()
{
	return _processed_count;
}

double Manager::RPC()
{
	auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _processed_time.load()).count();
	auto count = _processed_count.load();
	return msec != 0 ? (double)count * 1000.0 / (double)msec : 0;
}

} // namespace Logger