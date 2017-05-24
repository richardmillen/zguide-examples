#include <zmq.hpp>

#include <iostream>
using namespace std;

#if defined(WIN32)
#define sleep(n)		Sleep(n)
#endif

/*
publishes message envelopes where frame 1 is the key and frame 2 is the actual data.
*/

int main(int argc, char* argv[]) {
	zmq::context_t context(1);
	zmq::socket_t publisher(context, ZMQ_PUB);
	publisher.bind("tcp://*:5563");

	cout << "psenvpub: Publishing message envelopes..." << endl;

	while (true) {
		zmq::message_t key_frame(1);
		memcpy(key_frame.data(), "A", 1);
		publisher.send(key_frame, ZMQ_SNDMORE);

		zmq::message_t data_frame(25);
		memcpy(data_frame.data(), "We don't want to see this", 25);
		publisher.send(data_frame);

		key_frame.rebuild(1);
		memcpy(key_frame.data(), "B", 1);
		publisher.send(key_frame, ZMQ_SNDMORE);

		data_frame.rebuild(25);
		memcpy(data_frame.data(), "We would like to see this", 25);
		publisher.send(data_frame);

		sleep(1);
	}

	return 0;
}