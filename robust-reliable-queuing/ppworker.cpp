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

unique_ptr<zmq::socket_t> new_socket(zmq::context_t& context, const string& id);
string get_id();
inline void print_message(ostream& os, const string& id, const string& msg);

#define print_info(id, str)		print_message(cout, id, str)
#define print_warn(id, str)		print_message(cerr, id, str)
#define print_error(id, str)	print_message(cerr, id, str)

int main(int argc, char* argv[]) {
	if (!pp::check_version(4, 0))
		return 1;

	zmq::context_t context(1);

	auto id = get_id();
	auto worker = new_socket(context, id);

	auto heartbeat_at = 
		chrono::system_clock::now() + pp::heartbeat_interval;

	auto timeout = pp::heartbeat_interval;
	auto liveness = pp::heartbeat_liveness;

	random_device rd;
	mt19937 eng(rd());
	uniform_int_distribution<> distr(0, 5);

	auto cycles = 0;
	while (true) {
		zmq::pollitem_t items[] = { *worker, 0, ZMQ_POLLIN, 0 };
		zmq::poll(items, 1, timeout);

		if (items[0].revents & ZMQ_POLLIN) {
			/* Get message
			- 3-part envelope + content -> request
			- 1-part "HEARTBEAT" -> heartbeat */

			auto frames = pp::recv_frames(*worker);
			if (pp::was_interrupted(frames))
				break;

			if (frames.size() == 3) {
				++cycles;
				if (cycles > 10 && distr(eng) == 0) {
					print_info(id, "Simulating crash...");
					break;
				}
				else if (cycles > 5 && distr(eng) == 0) {
					print_info(id, "Simulating CPU overload...");
					this_thread::sleep_for(5s);
				}

				print_info(id, "Sending echo - '" + frames.back() + "'");
				pp::send_frames(*worker, frames);

				liveness = pp::heartbeat_liveness;
				this_thread::sleep_for(1s);
			}
			else if (frames.size() == 1 && frames.back().compare("HEARTBEAT") == 0) {
				liveness = pp::heartbeat_liveness;
			}
			else {
				print_error(id, "Invalid message.");
				pp::print_frames(frames);
				break;
			}

			// reset the timeout
			timeout = pp::heartbeat_interval;
		}
		else if (--liveness == 0) {
			print_warn(id, "Heartbeat failure, queue unreachable.");
			print_info(id, "Reconnecting in " + to_string(timeout.count()) + " seconds...");

			this_thread::sleep_for(timeout);

			if (timeout < pp::interval_max)
				timeout *= 2;

			worker = new_socket(context, id);
			liveness = pp::heartbeat_liveness;
		}

		if (chrono::system_clock::now() > heartbeat_at) {
			heartbeat_at =
				chrono::system_clock::now() + pp::heartbeat_interval;

			print_info(id, "Sending heartbeat...");

			worker->send("HEARTBEAT", 9);
		}
	}

	return 0;
}

unique_ptr<zmq::socket_t> new_socket(zmq::context_t& context, const string& id) {
	auto socket(make_unique<zmq::socket_t>(context, ZMQ_DEALER));

	socket->setsockopt(ZMQ_IDENTITY, id.c_str(), id.size());

	auto linger = 0;
	socket->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

	print_info(id, "Worker sending READY message...");

	socket->send("READY", 5);

	print_info(id, "Worker ready.");

	return socket;
}

string get_id() {
	return "ppworker-" + to_string(getpid());
}

void print_message(ostream& os, const string& id, const string& msg) {
	os << "ppworker (" << id << "): " << msg << endl;
}
