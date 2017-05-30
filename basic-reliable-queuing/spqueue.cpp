#include <zmq.hpp>

#include <iostream>
#include <tuple>
using namespace std;

void check_version(int want_major, int want_minor);

int main(int argc, char* argv[]) {
	check_version(2, 1);

	zmq::context_t context(1);
	zmq::socket_t frontend(context, ZMQ_ROUTER);
	zmq::socket_t backend(context, ZMQ_ROUTER);

	frontend.bind("tcp://*:5555");
	bactend.bind("tcp://*:5556");
	


	return 0;
}

void check_version(int want_major, int want_minor) {
	auto ver = zmq::version();

	if (get<0>(ver) > want_major)
		return;

	if (get<0>(ver) == want_major && get<1>(ver) >= want_minor)
		return;

	cerr << "Current 0MQ version is " << get<0>(ver) << "." << get<1>(ver) << ".\n" 
		<< "This application needs at least " << want_major << "." << want_minor << ".\n"
		<< "Cannot continue." << endl;

	exit(EXIT_FAILURE);
}

