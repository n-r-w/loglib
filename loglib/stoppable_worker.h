#pragma once
#include <future>

namespace Logger
{

// Базовый класс для корректного завершения потока
class StoppableWorker
{
public:
	StoppableWorker();

	//! Был ли запрос на остановку потока
	bool IsStopRequested() const;
	//! Запросить остановку потока
	virtual void StopRequest();

private:
	std::promise<void> exit_signal;
	std::future<void> exit_trigger;
};

} // namespace Logger
