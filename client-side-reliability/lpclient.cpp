#include <zmq.hpp>

#include <iostream>
#include <memory>
#include <chrono>
using namespace std;

#ifdef WIN32
#define sleep(n)		Sleep(n)
#endif

zmq::socket_t make_socket(zmq::context_t& context);

int main(int argc, char* argv[]) {

	// TODO: set these from command line arguments:
	auto retries_allowed = 3;
	auto timeout_secs = 3;

	zmq::context_t context(1);
	auto client = make_socket(context);

	auto sequence = 0;
	auto retries_left = retries_allowed;

	while (retries_left) {
		auto req_str(to_string(++sequence));

		cout << "lpclient: Sending request to server..." << endl;

		client.send(req_str.c_str(), req_str.size());

		auto expect_reply = true;
		while (expect_reply) {
			zmq::pollitem_t items[] = { client, 0, ZMQ_POLLIN, 0 };
			zmq::poll(items, 1, chrono::seconds(timeout_secs));

			if (items[0].revents & ZMQ_POLLIN) {
				// we got a reply from the server, must match sequence
				zmq::message_t reply;
				if (!client.recv(&reply))
					break;

				string rep_str(static_cast<char*>(reply.data()), reply.size());

				if (rep_str == req_str) {
					cout << "lpclient: Server replied OK (" << rep_str << ")" << endl;
					retries_left = retries_allowed;
					expect_reply = false;
				}
				else {
					cerr << "lpclient: Malformed reply from server. Expected: " << req_str << ", but was " << rep_str << "." << endl;
				}
			}
			else if (--retries_left == 0) {
				cerr << "lpclient: Server seems to be offline, abandoning..." << endl;
				// setting this variable is kind of pointless is it not?
				expect_reply = false;
				break;
			}
			else {
				cerr << "lpclient: No response from server, retrying..." << endl;
				client = make_socket(context);
				client.send(req_str.c_str(), req_str.size());
			}
		}
	}

	cout << "lpclient: Shutting down." << endl;

	return 0;
}

zmq::socket_t make_socket(zmq::context_t& context) {
	zmq::socket_t client(context, ZMQ_REQ);
	client.connect("tcp://localhost:5555");

	// "Pending messages shall be discarded immediately when the socket is closed with zmq_close()." 
	auto linger = 0;
	client.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

	return client;
}