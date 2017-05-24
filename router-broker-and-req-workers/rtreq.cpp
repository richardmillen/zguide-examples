#include <zmq.hpp>

#include <iostream>
#include <iomanip>
#include <vector>
#include <thread>
#include <chrono>
#include <string>
#include <sstream>
#include <random>
#include <mutex>
using namespace std;

#if defined(WIN32)
#define sleep(n)		Sleep(n)
#endif

mutex mut;

void worker_proc(size_t num);
void print_result(const string& id, unsigned int task_count);

int main(int argc, char* argv[]) {
	cout << "rtreq: Setting up ROUTER socket..." << endl;

	zmq::context_t context(1);
	zmq::socket_t broker(context, ZMQ_ROUTER);
	broker.bind("tcp://*:5671");

	cout << "rtreq: Starting 10 worker threads..." << endl;

	const auto total_workers = 10u;
	vector<thread> workers;
	for (size_t worker_nbr = 0; worker_nbr < 10; ++worker_nbr) {
		workers.push_back(thread(worker_proc, worker_nbr));
	}

	auto workers_fired = 0u;
	auto end_time = chrono::system_clock::now() + chrono::seconds(5);

	cout << "rtreq: Sending tasks to worker threads for 5 seconds..." << endl;

	while (workers_fired < total_workers) {
		// next message gives us least recently used worker
		zmq::message_t key_part;
		broker.recv(&key_part);
		string key(static_cast<char*>(key_part.data()), key_part.size());

		zmq::message_t delim_part;
		broker.recv(&delim_part);

		zmq::message_t reply_part;
		broker.recv(&reply_part);

		key_part.rebuild(key.size());
		memcpy(key_part.data(), key.c_str(), key.size());
		broker.send(key_part, ZMQ_SNDMORE);

		delim_part.rebuild(0);
		broker.send(delim_part, ZMQ_SNDMORE);

		string reply_str;
		if (chrono::system_clock::now() < end_time) {
			reply_str.assign("Work harder!");
		}
		else {
			reply_str.assign("You're fired!");
			++workers_fired;
		}

		reply_part.rebuild(reply_str.size());
		memcpy(reply_part.data(), reply_str.c_str(), reply_str.size());
		broker.send(reply_part);
	}

	for (auto& worker : workers)
		worker.join();

	cout << "rtreq: We're done!" << endl;
	getchar();

	return 0;
}

void worker_proc(size_t num) {
	ostringstream oss;
	oss << hex << uppercase << setw(4) << setfill('0') << num;
	auto id(oss.str());

	zmq::context_t context(1);
	zmq::socket_t worker(context, ZMQ_REQ);
	worker.setsockopt(ZMQ_IDENTITY, id.c_str(), id.size());
	worker.connect("tcp://localhost:5671");

	random_device rd;
	mt19937 eng(rd());
	uniform_int_distribution<> distr(1, 500);

	auto task_count = 0u;
	while (true) {
		zmq::message_t request(7);
		memcpy(request.data(), "Hi Boss", 7);
		worker.send(request);

		zmq::message_t reply;
		worker.recv(&reply);
		string rep_str(static_cast<char*>(reply.data()), reply.size());

		if (rep_str.compare("You're fired!") == 0)
			break;

		++task_count;
		sleep(distr(eng));
	}

	print_result(id, task_count);
}

void print_result(const string& id, unsigned int task_count) {
	unique_lock<decltype(mut)> lock(mut);
	cout << "rtreq: worker (" << id << "): Processed " << task_count << " tasks." << endl;
}
