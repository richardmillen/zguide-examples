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
inline string msg_to_str(zmq::message_t& message);
void print_msg(const string& who, const string& id, zmq::message_t& message);

int main(int argc, char* argv[]) {
	zmq::context_t context(1);
	zmq::socket_t frontend(context, ZMQ_ROUTER);
	zmq::socket_t backend(context, ZMQ_ROUTER);

	frontend.bind("tcp://*:5672");
	backend.bind("tcp://*:5673");

	cout << "lbbroker: Starting client threads..." << endl;

	vector<thread> clients;
	auto client_count = 10;
	for (size_t client_nbr = 0; client_nbr < client_count; ++client_nbr)
		clients.push_back(thread(client_proc, client_nbr));

	cout << "lbbroker: Starting worker threads..." << endl;

	vector<thread> workers;
	auto worker_count = 3;
	for (size_t worker_nbr = 0; worker_nbr < worker_count; ++worker_nbr)
		workers.push_back(thread(worker_proc, worker_nbr));

	cout << "lbbroker: Load balancer running..." << endl;

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

		// backend --> frontend
		if (items[0].revents & ZMQ_POLLIN) {
			zmq::message_t addr_part;
			backend.recv(&addr_part);
			worker_queue.push(msg_to_str(addr_part));

			zmq::message_t delim_part;
			backend.recv(&delim_part);
			assert(delim_part.size() == 0);

			addr_part.rebuild();
			backend.recv(&addr_part);
			auto client_addr(msg_to_str(addr_part));

			if (client_addr.compare("READY") != 0) {
				delim_part.rebuild();
				backend.recv(&delim_part);
				assert(delim_part.size() == 0);

				zmq::message_t reply_part;
				backend.recv(&reply_part);
				auto reply(msg_to_str(reply_part));

				addr_part.rebuild(client_addr.size());
				memcpy(addr_part.data(), client_addr.c_str(), client_addr.size());
				frontend.send(addr_part, ZMQ_SNDMORE);

				delim_part.rebuild(0);
				frontend.send(delim_part, ZMQ_SNDMORE);

				reply_part.rebuild(reply.size());
				memcpy(reply_part.data(), reply.c_str(), reply.size());
				frontend.send(reply_part);

				if (--client_count == 0)
					break;
			}
		}

		// frontend --> backend
		if (items[1].revents & ZMQ_POLLIN) {
			zmq::message_t addr_part;
			frontend.recv(&addr_part);
			auto client_addr(msg_to_str(addr_part));

			zmq::message_t delim_part;
			frontend.recv(&delim_part);
			assert(delim_part.size() == 0);

			zmq::message_t req_part;
			frontend.recv(&req_part);
			auto request(msg_to_str(req_part));

			auto worker_addr(worker_queue.front());
			worker_queue.pop();

			addr_part.rebuild(worker_addr.size());
			memcpy(addr_part.data(), worker_addr.c_str(), worker_addr.size());
			backend.send(addr_part, ZMQ_SNDMORE);

			delim_part.rebuild(0);
			backend.send(delim_part, ZMQ_SNDMORE);

			addr_part.rebuild(client_addr.size());
			memcpy(addr_part.data(), client_addr.c_str(), client_addr.size());
			backend.send(addr_part, ZMQ_SNDMORE);

			delim_part.rebuild(0);
			backend.send(delim_part, ZMQ_SNDMORE);

			req_part.rebuild(request.size());
			memcpy(req_part.data(), request.c_str(), request.size());
			backend.send(req_part);
		}
	}

	for (auto& client : clients)
		client.join();

	cout << "lbbroker: All client threads finished." << endl;
	cout << "lbbroker: Sending kill signal to worker threads..." << endl;

	while (worker_queue.size() > 0) {
		auto worker_addr(worker_queue.front());
		worker_queue.pop();

		zmq::message_t addr_part(worker_addr.size());
		memcpy(addr_part.data(), worker_addr.c_str(), worker_addr.size());
		backend.send(addr_part, ZMQ_SNDMORE);

		zmq::message_t delim_part(0);
		backend.send(delim_part, ZMQ_SNDMORE);

		zmq::message_t kill_part(4);
		memcpy(kill_part.data(), "KILL", 4);
		backend.send(kill_part);

		cout << "lbbroker: Worker '" << worker_addr << "' notified." << endl;
	}

	for (auto& worker : workers)
		worker.join();

	cout << "lbbroker: Worker threads all shut down." << endl;
	getchar();

	return 0;
}

void client_proc(const size_t num) {
	ostringstream oss;
	oss << hex << uppercase << setw(4) << setfill('0') << num;
	auto id(oss.str());

	zmq::context_t context(1);
	zmq::socket_t client(context, ZMQ_REQ);
	client.setsockopt(ZMQ_IDENTITY, id.c_str(), id.size());
	client.connect("tcp://localhost:5672");

	zmq::message_t req_msg(5);
	memcpy(req_msg.data(), "HELLO", 5);
	client.send(req_msg);

	zmq::message_t rep_msg;
	client.recv(&rep_msg);

	print_msg("Client", id, rep_msg);
}

void worker_proc(const size_t num) {
	ostringstream oss;
	oss << hex << uppercase << setw(4) << setfill('0') << num;
	auto id(oss.str());
	
	zmq::context_t context(1);
	zmq::socket_t worker(context, ZMQ_REQ);
	worker.setsockopt(ZMQ_IDENTITY, id.c_str(), id.size());
	worker.connect("tcp://localhost:5673");

	zmq::message_t req_msg(5);
	memcpy(req_msg.data(), "READY", 5);
	worker.send(req_msg);

	while (true) {
		zmq::message_t addr_part;
		worker.recv(&addr_part);
		auto client_addr(msg_to_str(addr_part));

		if (client_addr.compare("KILL") == 0)
			break;

		zmq::message_t delim_part;
		worker.recv(&delim_part);
		assert(delim_part.size() == 0);

		req_msg.rebuild();
		worker.recv(&req_msg);
		
		print_msg("Worker", id, req_msg);

		addr_part.rebuild(client_addr.size());
		memcpy(addr_part.data(), client_addr.c_str(), client_addr.size());
		worker.send(addr_part, ZMQ_SNDMORE);

		delim_part.rebuild(0);
		worker.send(delim_part, ZMQ_SNDMORE);

		zmq::message_t rep_part(2);
		memcpy(rep_part.data(), "OK", 2);
		worker.send(rep_part);
	}
}

inline string msg_to_str(zmq::message_t& message) {
	return string(static_cast<char*>(message.data()), message.size());
}

void print_msg(const string& who, const string& id, zmq::message_t& message) {
	unique_lock<decltype(mut)> lock(mut);
	cout << "lbbroker: " << who << " (" << id << "): " << msg_to_str(message) << endl;
}