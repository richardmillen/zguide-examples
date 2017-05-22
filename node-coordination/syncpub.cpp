#include <zmq.hpp>

#include <iostream>
using namespace std;

#if defined(WIN32)
#define sleep(n)		Sleep(n)
#endif

#define SUBSCRIBERS_EXPECTED 10

int main(int argc, char* argv[]) {
	zmq::context_t context(1);

	zmq::socket_t publisher(context, ZMQ_PUB);

	auto sndhwm = 0;
	publisher.setsockopt(ZMQ_SNDHWM, &sndhwm, sizeof(sndhwm));
	publisher.bind("tcp://*:5561");

	zmq::socket_t syncservice(context, ZMQ_REP);
	syncservice.bind("tcp://*:5562");

	cout << "syncpub: Synchronising with " << SUBSCRIBERS_EXPECTED << " clients..." << endl;

	auto subscribers = 0;
	while (subscribers < SUBSCRIBERS_EXPECTED) {
		zmq::message_t message;
		syncservice.recv(&message);

		message.rebuild(0);
		syncservice.send(message);

		subscribers++;
	}

	cout << "syncpub: Sending 1,000,000 updates to clients..." << endl;

	for (size_t update_nbr = 0; update_nbr < 1'000'000; ++update_nbr) {
		zmq::message_t message(7);
		memcpy(message.data(), "Rhubarb", 7);
		publisher.send(message);
	}

	cout << "syncpub: Notifying clients that we're done..." << endl;

	zmq::message_t message(3);
	memcpy(message.data(), "END", 3);
	publisher.send(message);

	sleep(1);

	cout << "syncpub: Shutting down..." << endl;

	return 0;
}
