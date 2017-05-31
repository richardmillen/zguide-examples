#include "ppcommon.hpp"
#include "workers.hpp"

#include <zmq.hpp>

#include <iostream>
#include <string>
#include <tuple>
#include <thread>
#include <chrono>
#include <map>
using namespace std;

bool check_version(int wants_major, int wants_minor);

static const unsigned HEARTBEAT_LIVENESS = 3; // 3-5 is reasonable
static const unsigned HEARTBEAT_INTERVAL = 1; // seconds

int main(int argc, char* argv[]) {
	if (!check_version(4, 0))
		return 1;

	zmq::context_t context(1);
	zmq::socket_t frontend(context, ZMQ_ROUTER);
	zmq::socket_t backend(context, ZMQ_ROUTER);
	frontend.bind("tcp://*:5555");
	backend.bind("tcp://*:5556");

	pp::workers_t workers;

	auto heartbeat_at = chrono::system_clock::now() + 
		chrono::seconds(HEARTBEAT_INTERVAL);
	
	while (true) {
		zmq::pollitem_t items[] = {
			{ backend, 0, ZMQ_POLLIN, 0 },
			{ frontend, 0, ZMQ_POLLIN, 0 }
		};

		// don't poll frontend if we don't have any workers available
		if (workers.empty())
			zmq::poll(items, 1, chrono::seconds(HEARTBEAT_INTERVAL));
		else
			zmq::poll(items, 2, chrono::seconds(HEARTBEAT_INTERVAL));

		if (items[0].revents & ZMQ_POLLIN) {
			auto frames = pp::recv_frames(backend);
			auto id(frames.front());
			frames.pop_front();
			
			assert(frames.front().empty());
			frames.pop_front();

			if (frames.size() == 1) {
				if (frames.front().compare("READY") == 0) {
					workers.erase(id);
					workers.push(id);
				}
				else if (frames.front().compare("HEARTBEAT") == 0) {
					workers.refresh(id);
				}
				else {
					cout << "ppqueue: Invalid message from " << id << endl;
					pp::print_frames(frames);
				}
			}
			else {
				pp::send_frames(frontend, frames);
				workers.push(id);
			}
		}

		if (items[1].revents & ZMQ_POLLIN) {
			auto frames = pp::recv_frames(frontend);
			frames.push_front(workers.front());
			workers.pop();

			pp::send_frames(backend, frames);
		}

		// send heartbeats to idle workers if it's time
		if (chrono::system_clock::now() > heartbeat_at) { 
			for (auto& w : workers) 
				backend.send(w.c_str(), w.size());

			heartbeat_at = chrono::system_clock::now() + 
				chrono::seconds(HEARTBEAT_INTERVAL);
		}
		// s_queue_purge(queue);
	}
	
	return 0;
}

bool check_version(int wants_major, int wants_minor) {
	auto ver = zmq::version();

	if (get<0>(ver) > wants_major)
		return true;
	if (get<0>(ver) == wants_major && get<1>(ver) >= wants_minor)
		return true;

	cerr << "You're using 0MQ " << get<0>(ver) << "." << get<1>(ver) << ".\n" <<
		"You need at least " << wants_major << "." << wants_minor << "." << endl;

	return false;
}
