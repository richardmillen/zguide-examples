#include <zmq.hpp>

#include <iostream>
#include <thread>
using namespace std;

/*
This is almost identical to the mtrelay example (http://zguide.zeromq.org/cpp:mtrelay),
demonstrating a sequential flow of execution, where each step is executed in its own thread.

This example differs in its use of std::thread (where the zguide doesn't), which means that
either join or detach must be called on each new thread or the destructor will call 
std::terminate() on the joinable (finished, but not joined) thread and throw an exception.

I chose to call detach() because the threads are meant to be independent, using 0mq messages
alone to synchronise the flow of execution.

n.b. Exception handling was added to thread procs as a reminder that using 'naked' std::thread's
is dangerous where the thread might throw an exception. This is especially true of attached threads.
*/

void step1(zmq::context_t* context) {
	try {
		zmq::socket_t sender(*context, ZMQ_PAIR);
		sender.connect("inproc://step2");

		zmq::message_t message(0);
		sender.send(message);
	}
	catch (zmq::error_t& e) {
		cerr << "mtrelay (step1): 0mq error: " << e.what() << endl;
	}
	catch (exception& e) {
		cerr << "mtrelay (step1): " << e.what() << endl;
	}
	catch (...) {
		cerr << "mtrelay (step1): An unknown error occurred." << endl;
	}
}

void step2(zmq::context_t* context) {
	try {
		zmq::socket_t receiver(*context, ZMQ_PAIR);
		receiver.bind("inproc://step2");

		thread s1(step1, context);
		s1.detach();

		zmq::message_t message;
		receiver.recv(&message);

		zmq::socket_t sender(*context, ZMQ_PAIR);
		sender.connect("inproc://step3");

		message.rebuild(0);
		sender.send(message);
	}
	catch (zmq::error_t& e) {
		cerr << "mtrelay (step2): 0mq error: " << e.what() << endl;
	}
	catch (exception& e) {
		cerr << "mtrelay (step2): " << e.what() << endl;
	}
	catch (...) {
		cerr << "mtrelay (step2): An unknown error occurred." << endl;
	}
}

int main(int argc, char* argv[]) {
	zmq::context_t context(1);

	cout << "mtrelay: Binding to final step in sequence..." << endl;

	zmq::socket_t receiver(context, ZMQ_PAIR);
	receiver.bind("inproc://step3");

	cout << "mtrelay: Starting sequential work..." << endl;

	thread s2(step2, &context);
	s2.detach();

	zmq::message_t message;
	receiver.recv(&message);

	cout << "mtrelay: Test completed!" << endl;
	getchar();

	return 0;
}