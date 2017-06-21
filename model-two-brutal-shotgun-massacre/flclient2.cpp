
/* Freelance client - Model 2
 * Uses DEALER socket to blast one or more services

 * The pros and cons of our shotgun approach are:
 * + Pro: it is simple, easy to make and easy to understand.
 * + Pro: it does the job of failover, and works rapidly, so long as there is at least one server running.
 * + Con: it creates redundant network traffic.
 * + Con: we can't prioritize our servers, i.e., Primary, then Secondary.
 * + Con: the server can do at most one request at a time, period. */

#include <zmq.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <deque>
#include <algorithm>
#include <chrono>
#include <locale>
#include <sstream>
using namespace std;

class flclient_t {
public:
	flclient_t(); 
	void connect(const string& endpoint); 
	deque<string> request(const string& request);

private:
	void send_frames(const deque<string>& frames);
	deque<string> recv_frames();
private:
	zmq::context_t context_;
	zmq::socket_t socket_;
	size_t servers_;
	unsigned sequence_;
};

// if not a single service replies within this time, give up
#define TOTAL_REQUESTS 10'000
#define GLOBAL_TIMEOUT chrono::milliseconds(2500)

/* n.b. minor mod to the main request loop where 'while (requests)'
 * is used rather than 'while (requests--)'. this means 'requests' 
 * will end up 0 not -1; useful for the report. */
int main(int argc, char* argv[]) {
	if (argc == 1) {
		cout << "Usage: flclient2 <endpoint>" << endl;
		return 0;
	}

	// for pretty printing numbers (i.e. with commas):
	cout.imbue(locale(""));

	flclient_t client;
	for (size_t argn = 1; argn < argc; ++argn) {
		cout << "flclient2: connecting to " << argv[argn] << "..." << endl;

		string ep("tcp://");
		ep.append(argv[argn]);
		client.connect(ep);
	}

	// send a bunch of name resolution 'requests', measure time
	auto requests = TOTAL_REQUESTS;
	auto start = chrono::system_clock::now();

	cout << "flclient2: sending " << requests << " requests to server..." << endl;

	while (requests) {
		auto reply { client.request("random name") };
		if (reply.empty()) {
			cerr << "flclient2: name service not available, aborting..." << endl;
			break;
		}
		--requests;
	}
	
	auto received_count = TOTAL_REQUESTS - requests;
	auto elapsed{ chrono::system_clock::now() - start };

	cout << "Received " << received_count << " replies." << endl;

	auto ms = chrono::duration_cast<chrono::milliseconds>(elapsed);
	auto ns = chrono::duration_cast<chrono::nanoseconds>(elapsed);
	cout << "Average round trip cost:\n\t" << ms.count() / received_count << " ms.\n\t" << ns.count() / received_count << " ns." << endl;

	return 0;
}

flclient_t::flclient_t()
	: context_{ 1 }, socket_{ context_, ZMQ_DEALER }, servers_{ 0 }, sequence_{ 0 }
{}

void flclient_t::connect(const string& endpoint) {
	socket_.connect(endpoint);
	++servers_;
}

/* This method does the hard work. It sends a request to all
 * connected servers in parallel (for this to work, all connections
 * must be successful and completed by this time). It then waits
 * for a single successful reply, and returns that to the caller.
 * Any other replies are just dropped.
 * 
 * n.b. I changed the main loop from a while to a do/while to
 * ensure execution (esp. useful when debugging). */
deque<string> flclient_t::request(const string& request) {
	deque<string> frames;

	// prefix request with sequence number and empty envelope
	frames.push_front(to_string(++sequence_));
	frames.push_front(string());

	// blast the request to all connected servers
	for (size_t server = 0; server < servers_; ++server)
		send_frames(frames);
	
	deque<string> reply;
	auto end_time = chrono::system_clock::now() + GLOBAL_TIMEOUT;
	auto remaining_ms = [&]() {
		auto remaining = end_time - chrono::system_clock::now();
		if (remaining.count() < 0)
			return chrono::milliseconds(0);
		return chrono::duration_cast<chrono::milliseconds>(remaining);
	};

	// wait for a matching reply to arrive from anywhere
	// since we can poll several times, calculate each one
	do {
		zmq::pollitem_t items[] = { socket_, 0, ZMQ_POLLIN, 0 };
		zmq::poll(items, 1, remaining_ms());
		if (items[0].revents & ZMQ_POLLIN) {
			// reply is [empty][sequence][OK]
			reply = recv_frames();
			assert(reply.size() == 3);
			reply.pop_front();
			auto seq_no = stoi(reply.front());
			if (seq_no == sequence_)
				break;
		}
	} while (chrono::system_clock::now() < end_time);

	return reply;
}

void flclient_t::send_frames(const deque<string>& frames) {
	auto stop_before_last = frames.end() - 1;
	std::for_each(frames.begin(), stop_before_last, [&](const string& frame) {
		socket_.send(frame.c_str(), frame.size(), ZMQ_SNDMORE);
	});

	socket_.send(frames.back().c_str(), frames.back().size());
}

deque<string> flclient_t::recv_frames() {
	deque<string> frames;

	auto more = TRUE;
	while (more) {
		zmq::message_t message;
		socket_.recv(&message);
		frames.push_back(string(static_cast<char*>(message.data()), message.size()));

		more = socket_.getsockopt<int>(ZMQ_RCVMORE);
	}
	return frames;
}

