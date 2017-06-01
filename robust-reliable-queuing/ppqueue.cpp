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

int main(int argc, char* argv[]) {
	if (!pp::check_version(4, 0))
		return 1;

	zmq::context_t context(1);
	zmq::socket_t frontend(context, ZMQ_ROUTER);
	zmq::socket_t backend(context, ZMQ_ROUTER);
	frontend.bind("tcp://*:5555");
	backend.bind("tcp://*:5556");

	pp::workers_t workers(pp::ttl);

	auto heartbeat_at = chrono::system_clock::now() +
		pp::heartbeat_interval;
	
	while (true) {
		zmq::pollitem_t items[] = {
			{ backend, 0, ZMQ_POLLIN, 0 },
			{ frontend, 0, ZMQ_POLLIN, 0 }
		};

		// don't poll frontend if we don't have any workers available
		if (workers.empty())
			zmq::poll(items, 1, pp::heartbeat_interval);
		else
			zmq::poll(items, 2, pp::heartbeat_interval);

		if (items[0].revents & ZMQ_POLLIN) {
			auto frames = pp::recv_frames(backend);
			if (pp::was_interrupted(frames))
				break;

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

		if (!workers.empty()) {
			// send heartbeats to idle workers if it's time
			if (chrono::system_clock::now() > heartbeat_at) {
				for (auto& id : workers)
					backend.send(id.c_str(), id.size());

				heartbeat_at = chrono::system_clock::now() +
					pp::heartbeat_interval;
			}

			workers.purge();
		}
	}
	
	return 0;
}
