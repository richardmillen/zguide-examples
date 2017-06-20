
#include <zmq.hpp>

#include <iostream>
#include <string>
#include <deque>
using namespace std;

deque<string> recv_frames(zmq::socket_t& socket);
void send_frames(zmq::socket_t& socket, const deque<string>& frames);

int main(int argc, char* argv[]) {
	if (argc < 2) {
		cerr << "Usage: flserver2 <endpoint>" << endl;
		return 0;
	}

	zmq::context_t context(1);
	zmq::socket_t server(context, ZMQ_REP);
	server.bind(argv[1]);

	cout << "flserver2: service is ready at " << argv[1] << "." << endl;

	while (true) {
		auto request{ recv_frames(server) };
		if (request.empty()) {
			cout << "flserver2: interrupted." << endl;
			break;
		}

		// fail nastily if run against wrong client
		assert(request.size() == 2);

		deque<string> reply;
		reply.push_back(request.front());
		reply.push_back("OK");

		send_frames(server, reply);
	}

	return 0;
}

deque<string> recv_frames(zmq::socket_t& socket) {

}

void send_frames(zmq::socket_t& socket, const deque<string>& frames) {

}