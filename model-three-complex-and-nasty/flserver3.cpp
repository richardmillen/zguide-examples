#include "flmsg.hpp"
using namespace fl3;

#include <zmq.hpp>

#include <boost/algorithm/string.hpp>

#include <iostream>
#include <string>
#include <deque>
using namespace std;

bool get_verbose(int argc, char* argv[]);
unsigned get_port(int argc, char* argv[]);

#define DEFAULT_PORT		5555

int main(int argc, char* argv[]) {
	auto verbose = get_verbose(argc, argv);
	auto port = get_port(argc, argv);

	string bind_addr("tcp://*:" + to_string(port));
	string conn_addr("tcp://localhost:" + to_string(port));

	zmq::context_t context(1);
	zmq::socket_t server(context, ZMQ_ROUTER);
	server.setsockopt(ZMQ_IDENTITY, conn_addr.c_str(), conn_addr.size());
	server.bind(bind_addr);

	cout << "flserver3: service is ready at " << conn_addr << "..." << endl;
	if (verbose)
		cout << "flserver3: running in verbose mode..." << endl;

	while (true) {
		auto request = utils::recv_msg(server);
		if (request.empty()) {
			cerr << "flserver3: interrupted." << endl;
			break;
		}

		if (verbose)
			utils::dump_msg("flserver3: request received by " + conn_addr, request);

		// Frame 0: identity of client
		// Frame 1: PING, or client control frame
		// Frame 2: request body
		auto identity = request.front();
		request.pop_front();

		auto control = request.front();
		request.pop_front();

		deque<string> reply;
		if (control == "PING") {
			reply.push_front("PONG");
		}
		else {
			reply.push_front("OK");
			reply.push_front(control);
		}
		reply.push_front(identity);

		if (verbose)
			utils::dump_msg("flserver3: reply sent by " + conn_addr, reply);

		utils::send_msg(server, reply);
	}

	return 0;
}

bool get_verbose(int argc, char* argv[]) {
	for (size_t argn = 1; argn < argc; ++argn)
		if (boost::iequals(argv[argn], "-v"))
			return true;
	
	return false;
}

/* get_port returns the first numeric argument
 * as a port number to be used by the server. */
unsigned get_port(int argc, char* argv[]) {
	for (size_t argn = 1; argn < argc; ++argn) {
		char* end;
		auto arg = argv[argn];
		auto port = strtol(argv[argn], &end, 10);
		if (arg != end)
			return port;
	}

	return DEFAULT_PORT;
}

