#include <zmq.hpp>

#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <sstream>
using namespace std;

#if defined(WIN32)
#define sleep(n)		Sleep(n)
#endif

mutex mut;
void sync_cout(string str) {
	unique_lock<decltype(mut)> lock(mut);
	cout << str << endl;
}

void worker_proc(zmq::context_t* context) {
	zmq::socket_t socket(*context, ZMQ_REP);
	socket.connect("inproc://workers");

	while (true) {
		sync_cout("mtserver (worker): Waiting to receive request message...");

		zmq::message_t request;
		socket.recv(&request);

		sync_cout("mtserver (worker): Received request.");

		sleep(1);

		zmq::message_t reply(6);
		memcpy(reply.data(), "World", 6);
		socket.send(reply);
	}
}

int main(int argc, char* argv[]) {
	zmq::context_t context(1);

	cout << "mtserver: Setting up tcp and inproc connections..." << endl;

	zmq::socket_t clients(context, ZMQ_ROUTER);
	clients.bind("tcp://*:5555");
	zmq::socket_t workers(context, ZMQ_DEALER);
	workers.bind("inproc://workers");

	sync_cout("mtserver: Starting worker threads...");

	vector<thread> threads;
	for (size_t thread_nbr = 0; thread_nbr < 5; ++thread_nbr) {
		threads.push_back(thread(worker_proc, &context));
	}

	sync_cout("mtserver: Running proxy...");

	zmq::proxy(clients, workers, nullptr);

	return 0;
}

