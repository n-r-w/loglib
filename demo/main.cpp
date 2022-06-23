#include <iostream>
#include <thread>
#include <fmtlib/format.h>

#include "manager.h"

#define TEST 0

void errorFunc(const std::string& error)
{
	static std::mutex m;
	m.lock();
	std::cerr << error << std::endl;
	m.unlock();
}

int main()
{
	// настройки менеджера логов
	const size_t auto_flush_size = 1000; // после какого размера очереди в буфере воркеры логов начнут принудительно слать их нас сервер
	const size_t packet_size = auto_flush_size; // сколько записей максимум можно слать за один раз
#if TEST
	const size_t work_thread_count = 1;
#else
	const size_t work_thread_count =  (size_t)std::thread::hardware_concurrency() / 2; // количество потоков на обработку логов
#endif                                                                                                                                                                                                                                                   \
	// Максимальный размер буфера, при котором новые записи будут отбрасываться. Необходимо для исключения переполнения памяти в случае,
	// когда количество вызовов AddRecord превышает скорость обработки буфера
	size_t max_buffer_size = work_thread_count * auto_flush_size;
	const bool concat_records = true; // упаковывать ли несколько записей в один json. отключение только для тестирования
	const std::string token = "dbda0fba4da680c615340d6faa2868eb5413c3b837640078b87149872257f842";

	// настройки теста
#if TEST
	const size_t test_thread_count = 1;
#else
	const size_t test_thread_count = (size_t)std::thread::hardware_concurrency() / 2; // количество клиентов, которые параллельно пишут логи
#endif
	const size_t sleep_rate_ms = 10;
	const size_t sleep_pow = 2; // настройка авторегулирования притормаживания клиентов чтобы они не забили буфер менеджера логов
	const size_t seconds = 0; // длительность теста

	Logger::Manager::Start(token, "localhost", 8080, work_thread_count, packet_size, auto_flush_size, max_buffer_size, concat_records, "");
	Logger::Manager::SetErrorFunc(errorFunc);
	Logger::Manager::EnableRPS(true);

	std::atomic<unsigned long long> num = 0;
	std::atomic_bool to_stop = false;
	std::vector<std::thread*> test_threads;
	std::atomic<size_t> sleep_rate_sum = 0;
	std::atomic<size_t> sleep_rate_count = 0;

	for (size_t i = 0; i < test_thread_count; i++)
	{
		test_threads.push_back(new std::thread([&]() {
			while (!to_stop)
			{
				auto r = std::make_shared<Logger::Record>();
				r->service = "WBA";
				r->source = "DEMO";
				r->category = "WBA";
				r->level = "INFO";
				r->session = "";
				r->info = "произвольная информация для поиска через регулярные выражения";
				r->url = "github.com/jackc/pgx/issues/771";
				r->httpType = "POST";
				r->properties = {{"idOrder", "123"}, {"idProject", "541"}};
				r->httpHeaders = {{"User-Agent", "PostmanRuntime/7.29.0"}};
				r->jsonBody =
R"(
{
    "id": "4286",
    "bank": {},
    "mainInfo": {
        "type": "personalDataChangePassportAge",
        "status": "anketaDraft"
    },
    "documentBase": {},
    "passportMain": {
        "gender": 1,
        "lastName": "Козлов",
        "birthDate": "1999-01-01",
        "firstName": "Андрей",
        "birthPlace": "Место рождения",
        "middleName": "Юрьевич",
        "registrationDate": "2022-06-01"
    },
    "applicant-flags": {
        "bankComplete": true,
        "bankNeedVerify": false,
        "overallComplete": false,
        "passportComplete": false,
        "documentBaseComplete": true,
        "documentBaseNeedVerify": false,
        "passport1stPageComplete": false,
        "passport2ndPageComplete": true,
        "passportNeedVerify_1stPage": true,
        "passportNeedVerify_2ndPage": true
    },
    "passportRegistration": {
        "area": null,
        "city": "г Москва",
        "flat": "кв 1",
        "house": "д 5",
        "index": "125319",
        "region": "г Москва",
        "street": "ул Часовая",
        "areaFiasGuid": null,
        "cityFiasGuid": "0c5b2444-70a0-4932-980c-b4dc0d3f02b5",
        "flatFiasGuid": "9eb55994-ab56-4df0-ba0b-798c27af0e91",
        "houseFiasGuid": "5e626110-547e-4947-b021-cbf8584658c0",
        "regionFiasGuid": "0c5b2444-70a0-4932-980c-b4dc0d3f02b5",
        "streetFiasGuid": "d8a334dd-3d3d-4838-aec1-41a40f616318"
    }
}
)";

				Logger::Manager::AddRecord(r);

				size_t total = Logger::Manager::BufferSize();
				size_t sleep_rate = (double)(pow(total, sleep_pow) * sleep_rate_ms) / (double)(auto_flush_size * test_thread_count);
				std::this_thread::sleep_for(std::chrono::milliseconds(sleep_rate));

				sleep_rate_sum += sleep_rate;
				sleep_rate_count++;

				num++;
			}
		}));
	}

	auto begin = std::chrono::steady_clock::now();
	while (seconds == 0 || std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - begin).count() < seconds)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		std::cout << fmt::format("RPC: {}. Total record created: {}. Sleep rate: {}. Buffer size: {}",
								 Logger::Manager::RPS(),
								 Logger::Manager::TotalProcessed(),
								 sleep_rate_count > 0 ? sleep_rate_sum / sleep_rate_count : 0, Logger::Manager::BufferSize()) << std::endl;
	}

	to_stop = true;
	for (size_t i = 0; i < test_thread_count; i++)
	{
		test_threads.at(i)->join();
	}

	Logger::Manager::Stop();

	return 0;
}
