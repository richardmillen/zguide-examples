// Espresso Pattern
// This shows how to capture data using pub-sub proxy

#include <zmq.hpp>

#include <iostream>
#include <thread>
#include <random>
using namespace std;

void subscriber_thread(zmq::context_t* context);
void publisher_thread(zmq::context_t* context);
void listener_thread(zmq::context_t* context);
zmq::socket_t make_socket(zmq::context_t& context, const int type);

int main(int argc, char* argv[]) {
	zmq::context_t context(1);

	thread pub_thread(publisher_thread, &context);		// ZMQ_PUB, binds, publishes on :6000
	thread sub_thread(subscriber_thread, &context);		// ZMQ_SUB, connects, subscribes to :6001

	auto sub{ make_socket(context, ZMQ_XSUB) };
	sub.connect("tcp://localhost:6000");

	auto pub{ make_socket(context, ZMQ_XPUB) };
	pub.bind("tcp://*:6001");

	auto lis_pipe{ make_socket(context, ZMQ_PAIR) };
	lis_pipe.bind("inproc://listener");
	thread lis_thread(listener_thread, &context);

	zmq::proxy(sub, pub, lis_pipe);

	cout << "interrupted" << endl;

	context.close();

	pub_thread.join();
	sub_thread.join();
	lis_thread.join();

	return 0;
}

/* the subscriber thread requests messages starting with
 * A and B, then reads and counts incoming messages. */
void subscriber_thread(zmq::context_t* context) {
	auto sub{ make_socket(*context, ZMQ_SUB) };
	sub.connect("tcp://localhost:6001");
	sub.setsockopt(ZMQ_SUBSCRIBE, "A", 1);
	sub.setsockopt(ZMQ_SUBSCRIBE, "B", 1);

	auto count = 0;
	while (count < 5) {
		zmq::message_t message;
		if (!sub.recv(&message))
			break;		// interrupted
		++count;
	}
}

/* the publisher sends random messages starting with A-J */
void publisher_thread(zmq::context_t* context) {
	auto pub{ make_socket(*context, ZMQ_PUB) };
	pub.bind("tcp://*:6000");

	random_device rd;
	mt19937 eng(rd());
	uniform_int_distribution<> distr((int)'A', (int)'J');

	auto counter = 0UL;
	while (true) {
		string content;
		content.append(1, (char)distr(eng));
		content.append("-");
		content.append(to_string(++counter));

		if (pub.send(content.c_str(), content.size()) == -1)
			break;		// interrupted

		this_thread::sleep_for(100ms);
	}
}

/* the listener receives all messages flowing through the proxy,
 * on its pipe. in czmq, the pipe is a pair of ZMQ_PAIR sockets 
 * that connect attached child threads. in other languages your
 * mileage may vary. */
void listener_thread(zmq::context_t* context) {
	auto pipe{ make_socket(*context, ZMQ_PAIR) };
	pipe.connect("inproc://listener");

	while (true) {
		zmq::message_t message;
		if (!pipe.recv(&message))
			break;		// interrupted

		string frame(static_cast<char*>(message.data()), message.size());
		cout << "espresso: listener: " << frame << endl;
	}
}

zmq::socket_t make_socket(zmq::context_t& context, const int type) {
	zmq::socket_t socket(context, type);
	socket.setsockopt<int>(ZMQ_LINGER, 0);
	return socket;
}