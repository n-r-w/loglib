#pragma once

#include <queue>
#include <thread>

#include "stoppable_worker.h"
#include "record.h"

namespace Logger
{

class Worker : public StoppableWorker
{
public:
	Worker(
		//! Токен доступа
		const std::string& token,
		//! Адрес сервера
		const std::string& host,
		//! Порт сервера
		uint16_t port,
		//! Сколько записей обрабатывать за один раз
		size_t packet_size,
		//! Максимальный размер буффера, после которого начнется его принудительное сбрасывание
		//! Если 0, то никогда
		size_t flush_buffer_size,
		//! При наличии в буфере нескольких записей, отправлять их одним пакетом
		bool concat_records);

	// Запуск на выполнение
	void Start(size_t number);

	//! Добавить запись
	void AddRecord(const RecordPtr& record);
	//! Остановить прием и обработать буффер
	void Flush();

	//! Размер текущей очереди на выполнение
	size_t BufferSize() const;

	//! Запросить остановку потока
	void StopRequest() override;

private:
	//! Обработчик записей
	bool ProcessRecords(const std::vector<RecordPtr>& records, int& error_code, std::string& error_text);
	//! Если не удалось выполнить ProcessRecords (например недоступен внешний сервис), то пишем ошибки в локальный файл
	void ProcessErrorRecords(const std::vector<RecordPtr>& records, int error_code, const std::string& error_text);
	//! Обработка буфера
	bool ProcessBuffer(size_t packet_size, bool full_lock);

	//! Отправка лога на удаленный сервер
	bool SendToServer(const std::vector<RecordPtr>& records, int& error_code, std::string& error_string);

	//! Токен доступа
	std::string _token;

	//! Адрес сервера
	std::string _host;
	//! Порт сервера
	uint16_t _port;

	size_t _number = 0;
	//! Сколько записей обрабатывать за один раз
	size_t _packet_size;
	//! Максимальный размер буффера, после которого начнется его принудительное сбрасывание
	//! Если 0, то никогда
	size_t _flush_buffer_size;

	mutable std::mutex _buffer_mutex;
	std::queue<RecordPtr> _buffer;

	std::condition_variable _wakeup;
	std::mutex _wakeup_mutex;

	bool _concat_records;
};

using WorkerPtr = std::shared_ptr<Worker>;

} // namespace Logger
