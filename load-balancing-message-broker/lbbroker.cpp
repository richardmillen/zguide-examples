#include <zmq.hpp>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <string>
#include <mutex>
#include <queue>
using namespace std;

#if defined(WIN32)
#define sleep(n)		Sleep(n)
#endif

mutex mut;

void client_proc(const size_t num);
void worker_proc(const size_t num);

int main(int argc, char* argv[]) {
	zmq::context_t context(1);
	zmq::socket_t frontend(context, ZMQ_ROUTER);
	zmq::socket_t backend(context, ZMQ_ROUTER);

	frontend.bind("tcp://*:5672");
	backend.bind("tcp://*:5673");

	vector<thread> clients;
	for (size_t client_nbr = 0; client_nbr < 10; ++client_nbr)
		clients.push_back(thread(client_proc, client_nbr));

	vector<thread> workers;
	for (size_t worker_nbr = 0; worker_nbr < 3; ++worker_nbr)
		workers.push_back(thread(worker_proc, worker_nbr));

	// Logic of LRU loop
	// - Poll backend always, frontend only if 1+ worker ready
	// - If worker replies, queue worker as ready and forward reply to client if necessary
	// - If client requests, pop next worker and send request to it
	// 
	// A very simple queue structure with known max size
	queue<string> worker_queue;

	while (true) {
		zmq::pollitem_t items[] = {
			{ backend, 0, ZMQ_POLLIN, 0 },
			{ frontend, 0, ZMQ_POLLIN, 0 }
		};

		if (worker_queue.size())
			zmq::poll(items, 2, -1);
		else
			zmq::poll(items, 1, -1);

		if (items[0].revents & ZMQ_POLLIN) {
			// http://zguide.zeromq.org/cpp:lbbroker
		}

		if (items[1].revents & ZMQ_POLLIN) {
			// http://zguide.zeromq.org/cpp:lbbroker
		}
	}

	for (auto& client : clients)
		client.join();

	for (auto& worker : workers)
		worker.join();

	return 0;
}

void client_proc(const size_t num) {
	// http://zguide.zeromq.org/cpp:lbbroker
}

void worker_proc(const size_t num) {
	// http://zguide.zeromq.org/cpp:lbbroker
}
