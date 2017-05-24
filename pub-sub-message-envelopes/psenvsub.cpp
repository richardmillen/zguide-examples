#include <zmq.hpp>

#include <iostream>
#include <string>
using namespace std;

int main(int argc, char* argv[]) {
	zmq::context_t context(1);
	zmq::socket_t subscriber(context, ZMQ_SUB);
	subscriber.connect("tcp://localhost:5563");
	subscriber.setsockopt(ZMQ_SUBSCRIBE, "B", 1);

	while (true) {
		zmq::message_t key_frame;
		subscriber.recv(&key_frame);
		string key(static_cast<char*>(key_frame.data()), key_frame.size());

		zmq::message_t data_frame;
		subscriber.recv(&data_frame);
		string data(static_cast<char*>(data_frame.data()), data_frame.size());

		cout << "[" << key << "] " << data << endl;
	}

	return 0;
}