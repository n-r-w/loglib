#pragma once
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <fstream>

#include "record.h"
#include "worker.h"

namespace Logger
{

class Manager : protected StoppableWorker
{
public:
	//! Запуск. Возвращает текст ошибки при невозможности логина к серверу
	static std::string Start(
		//! Токен доступа
		const std::string& token,
		//! Адрес сервера
		const std::string& host,
		//! Порт сервера
		uint16_t port,
		//! Количество обработчиков (потоков)
		size_t workers_count,
		//! Сколько записей обрабатывать за один раз
		size_t packet_size,
		//! Максимальный размер буффера, после которого начнется его принудительное сбрасывание
		//! Если 0, то никогда (возможно непредсказуемое использование памяти, если будет не успевать отправлять их на сервер логов)
		size_t flush_buffer_size,
		//! При наличии в буфере нескольких записей, сколько из них отправлять их одним пакетом на сервер логов
		bool concat_records,
		//! Имя файла, куда будут выводиться ошибки при невозможности отправки лога обычным способом
		//! Если не задано, то игнорируется
		const std::string& error_file_name);
	//! Остановка
	static void Stop();
	//! Добавить запись
	static void AddRecord(const RecordPtr& record);
	//! Суммарный размер буфера
	static size_t BufferSize();

	//! Вывод в консоль для тестирования
	static void CoutPrint(const std::string& message);
	//! Если не удалось выполнить ProcessRecords (например недоступен внешний сервис), то пишем ошибки в локальный файл
	static void SaveErrors(const std::vector<RecordPtr>& records, int error_code, const std::string& error_text);

	static void RegisterProcessedCount(uint64_t n);
	static uint64_t TotalProcessed();
	static double RPC();

private:
	//! Запуск
	void StartHelper(
		//! Токен доступа
		const std::string& token,
		//! Адрес сервера
		const std::string& host,
		//! Порт сервера
		uint16_t port,
		//! Количество обработчиков (потоков)
		size_t workers_count,
		//! Сколько записей обрабатывать за один раз
		size_t packet_size,
		//! Максимальный размер буффера, после которого начнется его принудительное сбрасывание
		//! Если 0, то никогда
		size_t flush_buffer_size,
		//! При наличии в буфере нескольких записей, отправлять их одним пакетом
		bool concat_records);
	//! Остановка
	void StopHelper();
	//! Добавить запись
	void AddRecordHelper(const RecordPtr& record);
	//! Суммарный размер буфера
	size_t BufferSizeHelper() const;

	std::vector<WorkerPtr> _workers;
	std::vector<std::unique_ptr<std::thread>> _worker_threads;
	std::atomic_bool _started = false;


	std::string _token;
	static std::string _host;
	static uint16_t _port;

	static std::mutex _manager_mutex;
	static std::shared_ptr<Manager> _manager;
	static std::unique_ptr<std::thread> _manager_thread;

	//! Блокировка параллельного вывода в консоль
	static std::mutex _cout_locker;

	//! Блокировка параллельного вывода в файл
	static std::mutex _file_locker;
	//! Имя файла, куда будут выводиться ошибки при невозможности отправки лога обычным способом
	static std::string _error_file_name;
	//! Файл журнала
	static std::ofstream _log_file;

	static std::atomic<int64_t> _processed_count;
	static std::atomic<std::chrono::steady_clock::time_point> _processed_time;
};

using ManagerPtr = std::shared_ptr<Manager>;

} // namespace Logger
