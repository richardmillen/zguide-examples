#include "spcommon.h"

#include <zmq.hpp>

#include <iostream>
#include <string>
#include <tuple>
#include <queue>
#include <deque>
using namespace std;

static const unsigned MAX_WORKERS = 100;

void check_version(int want_major, int want_minor);

int main(int argc, char* argv[]) {
	check_version(2, 1);

	zmq::context_t context(1);
	zmq::socket_t frontend(context, ZMQ_ROUTER);
	zmq::socket_t backend(context, ZMQ_ROUTER);

	frontend.bind("tcp://*:5555");
	backend.bind("tcp://*:5556");
	
	queue<string> worker_queue;

	while (true) {
		zmq::pollitem_t items[] = {
			{ backend, 0, ZMQ_POLLIN, 0 },
			{ frontend, 0, ZMQ_POLLIN, 0 }
		};

		// poll frontend only if we have available workers
		if (worker_queue.size())
			zmq::poll(items, 2, -1);
		else
			zmq::poll(items, 1, -1);

		if (items[0].revents & ZMQ_POLLIN) {
			cout << "spqueue: Received message from worker..." << endl;

			auto frames = sp::recv_frames(backend);

			assert(worker_queue.size() < MAX_WORKERS);
			
			worker_queue.push(frames.front());
			frames.pop_front();

			assert(frames.front().empty());
			frames.pop_front();

			if (frames.front().compare("READY") != 0) {
				cout << "spqueue: Forwarding to client..." << endl;

				sp::send_frames(frontend, frames);
			}
		}

		if (items[1].revents & ZMQ_POLLIN) {
			cout << "spqueue: Received message from client..." << endl;

			auto frames = sp::recv_frames(frontend);

			frames.push_front(string());
			frames.push_front(worker_queue.front());
			worker_queue.pop();

			cout << "spqueue: Forwarding to worker..." << endl;

			sp::send_frames(backend, frames);
		}
	}

	return 0;
}

void check_version(int want_major, int want_minor) {
	auto ver = zmq::version();

	if (get<0>(ver) > want_major)
		return;

	if (get<0>(ver) == want_major && get<1>(ver) >= want_minor)
		return;

	cerr << "Current 0MQ version is " << get<0>(ver) << "." << get<1>(ver) << ".\n"
		<< "This application needs at least " << want_major << "." << want_minor << ".\n"
		<< "Cannot continue." << endl;

	exit(EXIT_FAILURE);
}

