#include "ppcommon.hpp"

#include <zmq.hpp>

#include <iostream>
#include <memory>
#include <chrono>
#include <random>
#include <thread>
using namespace std;

#if defined(WIN32)
#include <process.h>
#define getpid()		::_getpid()
#endif

zmq::socket_t new_socket(zmq::context_t& context, const string& id);
string get_id();

int main(int argc, char* argv[]) {
	if (!pp::check_version(4, 0))
		return 1;

	zmq::context_t context(1);

	auto id = get_id();
	auto worker = new_socket(context, id);

	auto heartbeat_at = 
		chrono::system_clock::now() +
		pp::heartbeat_interval;

	auto timeout = pp::heartbeat_interval;
	auto liveness = pp::heartbeat_liveness;

	random_device rd;
	mt19937 eng(rd());
	uniform_int_distribution<> distr(0, 5);

	auto cycles = 0;
	while (true) {
		zmq::pollitem_t items[] = { worker, 0, ZMQ_POLLIN, 0 };
		zmq::poll(items, 1, pp::heartbeat_interval);

		if (items[0].revents & ZMQ_POLLIN) {
			/* Get message
			- 3-part envelope + content -> request
			- 1-part "HEARTBEAT" -> heartbeat */

			auto frames = pp::recv_frames(worker);
			if (pp::was_interrupted(frames))
				break;

			if (frames.size() == 3) {
				++cycles;
				if (cycles > 10 && distr(eng) == 0) {
					pp::print_info(id, "Simulating crash...");
					break;
				}
				else if (cycles > 5 && distr(eng) == 0) {
					pp::print_info(id, "Simulating CPU overload...");
					this_thread::sleep_for(5s);
				}

				pp::print_info(id, "Doing some \"heavy work\"...");
				this_thread::sleep_for(1s);
				
				pp::print_info(id, "Sending echo - '" + frames.back() + "'");
				pp::send_frames(worker, frames);
				liveness = pp::heartbeat_liveness;
			}
			else if (frames.size() == 1 && frames.back().compare("HEARTBEAT") == 0) {
				pp::print_debug(id, "Received heartbeat from queue.");
				liveness = pp::heartbeat_liveness;
			}
			else {
				pp::print_error(id, "Invalid message.");
				pp::print_frames(frames);
				break;
			}

			// reset the timeout
			timeout = pp::heartbeat_interval;
		}
		else if (--liveness == 0) {
			pp::print_warn(id, "Heartbeat failure, queue unreachable.");
			pp::print_info(id, "Reconnecting in " + to_string(timeout.count()) + " seconds...");

			this_thread::sleep_for(timeout);

			if (timeout < pp::interval_max)
				timeout *= 2;

			worker = new_socket(context, id);
			liveness = pp::heartbeat_liveness;
		}

		if (chrono::system_clock::now() > heartbeat_at) {
			heartbeat_at =
				chrono::system_clock::now() + 
				pp::heartbeat_interval;

			pp::print_debug(id, "Sending heartbeat...");

			worker.send("HEARTBEAT", 9);
		}
	}

	return 0;
}

zmq::socket_t new_socket(zmq::context_t& context, const string& id) {
	zmq::socket_t socket(context, ZMQ_DEALER);

	socket.setsockopt(ZMQ_IDENTITY, id.c_str(), id.size());
	socket.connect("tcp://localhost:5556");

	auto linger = 0;
	socket.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

	pp::print_debug(id, "Worker sending READY message...");

	socket.send("READY", 5);

	pp::print_info(id, "Worker ready.");

	return socket;
}

string get_id() {
	return "ppworker-" + to_string(getpid());
}
