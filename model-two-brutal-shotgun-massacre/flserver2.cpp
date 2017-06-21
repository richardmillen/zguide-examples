// Freelance server - Model 2
// Does some work, replies OK, with message sequencing

#include <zmq.hpp>

#include <iostream>
#include <string>
#include <deque>
using namespace std;

deque<string> recv_frames(zmq::socket_t& socket);
void send_frames(zmq::socket_t& socket, const deque<string>& frames);

int main(int argc, char* argv[]) {
	if (argc < 2) {
		cerr << "Usage: flserver2 <port>" << endl;
		return 0;
	}

	zmq::context_t context(1);
	zmq::socket_t server(context, ZMQ_REP);
	
	string addr("tcp://*:");
	addr.append(argv[1]);

	server.bind(addr);

	cout << "flserver2: service is listening on port " << argv[1] << "." << endl;

	while (true) {
		auto request{ recv_frames(server) };
		if (request.empty()) {
			cout << "flserver2: interrupted." << endl;
			break;
		}

		// fail nastily if run against wrong client
		// confusing - shouldn't the request size be one (not two as it is in example)?
		assert(request.size() == 1);

		deque<string> reply;
		reply.push_back(request.front());
		reply.push_back("OK");

		send_frames(server, reply);
	}

	return 0;
}

deque<string> recv_frames(zmq::socket_t& socket) {
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
	for (auto& it = frames.cbegin(); it < frames.cend() - 1; ++it){
		socket.send(it->c_str(), it->size(), ZMQ_SNDMORE);
	}
	socket.send(frames.back().c_str(), frames.back().size());
}