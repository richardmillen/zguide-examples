#include "spcommon.h"

#include <zmq.hpp>

#include <iostream>
#include <string>
#include <random>
#include <thread>
using namespace std;

#ifdef WIN32
#include <process.h>
#define getpid()		::_getpid()
#else
#define getpid()		::getpid()
#endif

string set_id(zmq::socket_t& socket);

int main(int argc, char* argv[]) {
	zmq::context_t context(1);
	zmq::socket_t worker(context, ZMQ_REQ);
	auto id = set_id(worker);
	worker.connect("tcp://localhost:5556");

	random_device rd;
	mt19937 eng(rd());
	uniform_int_distribution<> distr(0, 5);

	cout << "spworker: (" << id << "): Sending 'READY' message to broker..." << endl;
	worker.send("READY", 5);

	auto cycles = 0;
	while (true) {
		auto frames = sp::recv_frames(worker);

		++cycles;
		if (cycles > 6 && distr(eng) == 0) {
			cout << "spworker (" << id << "): Simulating a crash..." << endl;
			break;
		}
		else if (cycles > 3 && distr(eng) == 0) {
			cout << "spworker (" << id << "): Simulating CPU overload..." << endl;
			this_thread::sleep_for(5s);
		}

		cout << "spworker (" << id << "): Normal reply. Message body: '" << frames.back() << "'." << endl;
		
		// do some work then send reply
		this_thread::sleep_for(1s);
		sp::send_frames(worker, frames);

		cout << "spworker (" << id << "): Message sent." << endl;
	}

	return 0;
}

string set_id(zmq::socket_t& socket) {
	string id("spworker-" + to_string(getpid()));
	socket.setsockopt(ZMQ_IDENTITY, id.c_str(), id.size());
	return id;
}