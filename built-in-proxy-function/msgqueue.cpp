#include <zmq.hpp>

#include <iostream>
using namespace std;

int main(int argc, char* argv[]) {
	zmq::context_t context(1);

	zmq::socket_t frontend(context, ZMQ_ROUTER);
	frontend.bind("tcp://*:5559");

	zmq::socket_t backend(context, ZMQ_DEALER);
	backend.bind("tcp://*:5560");

	zmq::proxy(frontend, backend, nullptr);

	return 0;
}

