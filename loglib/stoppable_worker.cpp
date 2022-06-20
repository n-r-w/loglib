#include "stoppable_worker.h"

namespace Logger
{

StoppableWorker::StoppableWorker() : exit_trigger(exit_signal.get_future())
{
}

bool StoppableWorker::IsStopRequested() const
{
	return exit_trigger.wait_for(std::chrono::milliseconds(0)) != std::future_status::timeout;
}

void StoppableWorker::StopRequest()
{
	exit_signal.set_value();
}

} // namespace Logger
