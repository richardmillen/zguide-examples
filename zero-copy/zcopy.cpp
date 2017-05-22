#include <zmq.hpp>

#include <iostream>
#include <vector>
#include <memory>
#include <string>
using namespace std;

void no_free(void* data, void* hint) {
	cout << "zcopy: 0mq is telling us to free the memory buffer!" << endl;
}

int main(int argc, char* argv[]) {
	zmq::context_t context(1);
	zmq::socket_t publisher(context, ZMQ_PUB);
	publisher.bind("tcp://*:5555");

	auto big_buff = make_unique<char[]>(1'000'000); 

	// ...
	// don't bother initialising the array, it's just an example.
	// ...

	cout << "zcopy: Sending big-ass message 100 times, using zero-copy..." << endl;

	for (size_t i = 0; i < 100; ++i)
	{
		zmq::message_t message(big_buff.get(), 1'000'000, no_free);
		publisher.send(message);
	}

	cout << "zcopy: Finished publishing." << endl;
	getchar();

	return 0;
}
