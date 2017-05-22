#include <zmq.hpp>

#include <iostream>
#include <string>
using namespace std;

int main(int argc, char* argv[]) {
	zmq::context_t context(1);

	zmq::socket_t subscriber(context, ZMQ_SUB);
	subscriber.connect("tcp://localhost:5561");
	subscriber.setsockopt(ZMQ_SUBSCRIBE, "", 0);

	zmq::socket_t syncclient(context, ZMQ_REQ);
	syncclient.connect("tcp://localhost:5562");

	cout << "syncsub: Synchronising with publisher..." << endl;

	zmq::message_t message(0);
	syncclient.send(message);

	message.rebuild();
	syncclient.recv(&message);

	cout << "syncsub: Ready to receive updates." << endl;

	auto update_nbr = 0;
	while (true) {
		zmq::message_t message;
		subscriber.recv(&message);
		string str(static_cast<char*>(message.data()), message.size());

		if (str.compare("END") == 0) {
			break;
		}

		++update_nbr;
	}

	cout << "syncsub: Received " << update_nbr << " updates" << endl;

	return 0;
}
