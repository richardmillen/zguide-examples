#include <zmq.hpp>

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

#include <iostream>
#include <memory>
using namespace std;

/*
The signal_handler function is called when the user presses Ctrl+C.
However, the call to socket.recv doesn't return (at least not on windows) 
therefore it's necessary to set a short receive timeout on the socket so that
our code can react to the 'interrupted' flag.

An almost identical approach could be taken when using the zmq::poll function.
*/

static bool interrupted = false;

void signal_handler(int sig) {
	interrupted = true;
}

int main(int argc, char* argv[]) {
	zmq::context_t context(1);
	zmq::socket_t receiver(context, ZMQ_REP);
	receiver.setsockopt(ZMQ_RCVTIMEO, 100);
	receiver.bind("tcp://*:5555");

	signal(SIGINT, &signal_handler);
	signal(SIGTERM, &signal_handler);

	cout << "interrupt: Ready to receive request messages." << endl;
	
	while (!interrupted) {
		zmq::message_t message;
		try {
			if (receiver.recv(&message)) {
				// we've got something!
			}
		}
		catch (zmq::error_t& e) {
			cout << "interrupt: A 0mq error occurred: " << e.what() << endl;
		}
		catch (...) {
			cout << "interrupt: An unknown error occurred." << endl;
		}
		if (interrupted) {
			cout << "interrupt: Interrupted by the user." << endl;
		}
	}

	cout << "interrupt: Shutting down..." << endl;

	return 0;
}


