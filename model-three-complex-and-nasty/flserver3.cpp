#include "flmsg.hpp"
using namespace fl3;

#include <zmq.hpp>

#include <boost/algorithm/string.hpp>

#include <iostream>
#include <string>
#include <deque>
using namespace std;

//deque<string> recv_frames(zmq::socket_t& socket);
//void send_frames(zmq::socket_t& socket, const deque<string>& frames);
//void dump_frames(const deque<string>& frames);

int main(int argc, char* argv[]) {
	bool verbose = (argc > 1 && boost::iequals(argv[1], "-v"));

	string bind_addr("tcp://*:5555");
	string conn_addr("tcp://localhost:5555");

	zmq::context_t context(1);
	zmq::socket_t server(context, ZMQ_ROUTER);
	server.setsockopt(ZMQ_IDENTITY, conn_addr.c_str(), conn_addr.size());
	server.bind(bind_addr);

	cout << "flserver3: service is ready at " << conn_addr << "..." << endl;

	while (true) {
		auto request = utils::recv_msg(server);
		if (request.empty()) {
			cerr << "flserver3: interrupted." << endl;
			break;
		}

		if (verbose)
			utils::dump_msg(request);

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
			utils::dump_msg(reply);

		utils::send_msg(server, reply);
	}

	return 0;
}

/*deque<string> recv_frames(zmq::socket_t& socket) {
	deque<string> frames;

	do {
		zmq::message_t message;
		if (!socket.recv(&message))
			return deque<string>();

		frames.push_back(string(static_cast<char*>(message.data()), message.size()));

	} while (socket.getsockopt<int>(ZMQ_RCVMORE));

	return frames;
}

void send_frames(zmq::socket_t& socket, const deque<string>& frames) {
	auto is_last = [&](auto it) {
		return (it != frames.end()) && ((it + 1) == frames.end());
	};
	auto send_next = [&](auto next) {
		if (is_last(next)) {
			socket.send(next->c_str(), next->size());
		}
		else {
			socket.send(next->c_str(), next->size(), ZMQ_SNDMORE);
		}
	};
	send_next(frames.begin());
}

void dump_frames(const deque<string>& frames) {
	// TODO: implement dump
}*/