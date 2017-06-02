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

static const char* app_id = "ppqueue";

int main(int argc, char* argv[]) {
	if (!pp::check_version(4, 0))
		return 1;

	zmq::context_t context(1);
	zmq::socket_t frontend(context, ZMQ_ROUTER);
	zmq::socket_t backend(context, ZMQ_ROUTER);
	frontend.bind("tcp://*:5555");
	backend.bind("tcp://*:5556");

	pp::print_info(app_id, "'Paranoid Pirate' queue running...");

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
			
			if (frames.size() == 1) {
				if (frames.front().compare("READY") == 0) {
					pp::print_info(app_id, "Worker " + id + " is ready.");

					workers.erase(id);
					workers.push(id);
				}
				else if (frames.front().compare("HEARTBEAT") == 0) {
					/* TODO:
					what if a worker is just too slow in sending their next heartbeat.
					refreshing a worker that's been purged won't work. */
					workers.refresh(id);
				}
				else {
					pp::print_warn(app_id, "Invalid message from " + id + ".");
					pp::print_frames(frames);
				}
			}
			else {
				pp::print_debug(app_id, "Forwarding reply from worker back to client...");
				pp::print_frames(frames);

				pp::send_frames(frontend, frames);
				workers.try_push(id);

				pp::print_debug(app_id, "Reply sent.");
			}
		}

		if (items[1].revents & ZMQ_POLLIN) {
			pp::print_debug(app_id, "Received request from client.");

			auto frames = pp::recv_frames(frontend);

			pp::print_debug(app_id, "Forwarding request to worker " + workers.front() + "...");

			frames.push_front(workers.front());
			workers.pop();

			pp::send_frames(backend, frames);

			pp::print_debug(app_id, "Request sent.");
		}

		if (!workers.empty()) {
			if (chrono::system_clock::now() > heartbeat_at) {
				for (auto& id : workers) {
					deque<string> frames{ id, "HEARTBEAT" };
					pp::send_frames(backend, frames);
				}

				heartbeat_at = chrono::system_clock::now() +
					pp::heartbeat_interval;
			}

			/* ASSUMPTION: 
			workers will be purged if it is they who are too slow to send heartbeats,
			not that the lack of heartbeats was caused by a pause in this queue for some reason. */
			workers.purge();
		}
	}
	
	return 0;
}
