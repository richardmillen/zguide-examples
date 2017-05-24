#include <zmq.hpp>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <chrono>
#include <thread>
#include <random>
#include <mutex>
using namespace std;

#if defined(WIN32)
#define sleep(n)		Sleep(n)
#endif

mutex mut;

void worker_proc(size_t num);
void print_result(const string& id, unsigned int total_done);

int main(int argc, char* argv[]) {
	zmq::context_t context(1);
	zmq::socket_t broker(context, ZMQ_ROUTER);

	broker.bind("tcp://*:5671");

	const auto total_workers = 10u;
	vector<thread> workers;
	for (size_t worker_nbr = 0; worker_nbr < total_workers; ++worker_nbr) {
		workers.push_back(thread(worker_proc, worker_nbr));
	}

	auto workers_fired = 0u;
	auto end_time = chrono::system_clock::now() + chrono::seconds(5);

	cout << "rtdealer: Distributing tasks to worker threads for 5 seconds..." << endl;

	while (workers_fired < total_workers) {
		zmq::message_t id_msg;
		broker.recv(&id_msg);
		string id(static_cast<char*>(id_msg.data()), id_msg.size());

		zmq::message_t delim_msg;
		broker.recv(&delim_msg);

		zmq::message_t reply_msg;
		broker.recv(&reply_msg);

		id_msg.rebuild(id.size());
		memcpy(id_msg.data(), id.c_str(), id.size());
		broker.send(id_msg, ZMQ_SNDMORE);

		delim_msg.rebuild(0);
		broker.send(delim_msg, ZMQ_SNDMORE);

		string reply;
		if (chrono::system_clock::now() < end_time) {
			reply.assign("Work harder");
		}
		else {
			reply.assign("Fired!");
			++workers_fired;
		}

		reply_msg.rebuild(reply.size());
		memcpy(reply_msg.data(), reply.c_str(), reply.size());
		broker.send(reply_msg);
	}

	for (auto& worker : workers)
		worker.join();

	cout << "Finished!" << endl;
	getchar();

	return 0;
}

void worker_proc(size_t num) {
	ostringstream oss;
	oss << hex << uppercase << setw(4) << setfill('0') << num;
	auto id(oss.str());

	zmq::context_t context(1);
	zmq::socket_t worker(context, ZMQ_DEALER);
	worker.setsockopt(ZMQ_IDENTITY, id.c_str(), id.size());
	worker.connect("tcp://localhost:5671");

	random_device rd;
	mt19937 eng(rd());
	uniform_int_distribution<> distr(1, 500);

	auto total = 0u;
	while (true) {
		zmq::message_t delim_msg(0);
		worker.send(delim_msg, ZMQ_SNDMORE);

		zmq::message_t content_msg(7);
		memcpy(content_msg.data(), "Hi Boss", 7);
		worker.send(content_msg);

		delim_msg.rebuild();
		worker.recv(&delim_msg);

		content_msg.rebuild();
		worker.recv(&content_msg);
		string workload(static_cast<char*>(content_msg.data()), content_msg.size());

		if (workload.compare("Fired!") == 0)
			break;

		++total;
		sleep(distr(eng));
	}

	print_result(id, total);
}

void print_result(const string& id, unsigned int total_done) {
	unique_lock<decltype(mut)> lock(mut);
	cout << "rtdealer: worker (" << id << "): Processed " << total_done << " tasks." << endl;
}