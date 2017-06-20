
#include <zmq.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <deque>
#include <chrono>
using namespace std;

class flclient_t {
public:
	flclient_t(); 
	void connect(const string& endpoint); 
	deque<string> send_request(const string& request);

private:
	void send_frames(const deque<string>& frames);
	deque<string> recv_frames();
private:
	unique_ptr<zmq::context_t> context_;
	unique_ptr<zmq::socket_t> socket_;
	size_t servers_;
	unsigned sequence_;
};

// if not a single service replies within this time, give up
#define GLOBAL_TIMEOUT 2500

int main(int argc, char* argv[]) {
	if (argc == 1) {
		cout << "Usage: flclient2 <endpoint>" << endl;
		return 0;
	}

	flclient_t client;

	for (size_t argn = 1; argn < argc; ++argn)
		client_connect(client, argv[argn]);

	// send a bunch of name resolution 'requests', measure time
	auto requests = 10'000;
	auto start = chrono::system_clock::now();
	while (requests--) {
		auto reply { client_request(client, "random name") };
		if (reply.empty()) {
			cerr << "flclient2: name service not available, aborting..." << endl;
			break;
		}
	}
	auto elapsed { chrono::system_clock::now() - start };
	cout << "Average round trip cost: " << elapsed.count() << " ms" << endl;

	return 0;
}

flclient_t::flclient_t()
	: context_( make_unique<zmq::context_t>(1)) ,
	socket_( make_unique<zmq::socket_t>(context_, ZMQ_DEALER)) {}

void flclient_t::connect(const string& endpoint) {
	socket_.connect(endpoint);
	++servers_;
}

/* This method does the hard work. It sends a request to all
 * connected servers in parallel (for this to work, all connections
 * must be successful and completed by this time). It then waits
 * for a single successful reply, and returns that to the caller.
 * Any other replies are just dropped. */
deque<string> flclient_t::send_request(const string& request) {
	deque<string> frames;

	// prefix request with sequence number and empty envelope
	frames.push_back(to_string(++sequence_));
	frames.push_back(string());

	// blast the request to all connected servers
	for (size_t server = 0; server < servers_; ++server)
		send_frames(frames);
	
	// wait for a matching reply to arrive from anywhere
	// since we can poll several times, calculate each one
	// TODO: this is mostly wrong - make it right!
	deque<string> reply;
	auto end_time = chrono::system_clock::now() + GLOBAL_TIMEOUT;
	while (chrono::system_clock_now() < end_time) {
		zmq::pollitem_t items[] = { socket_, 0, ZMQ_POLLIN, 0 };
		zmq::poll(items, 1, end_time - chrono::system_clock::now() * ZMQ_POLL_MSEC);
		if (items[0].revents & ZMQ_POLLIN) {
			// reply is [empty][sequence][OK]
			reply = recv_frames();
			assert(reply.size() == 3);
			reply.pop_front();
			auto seq_no = stoi(reply.front());
			if (seq_no == sequence_)
				break;
		}
	}
	return reply;
}

void flclient_t::send_frames(const deque<string>& frames) {
}

deque<string> flclient_t::recv_frames() {
}

