
#include <zmq.hpp>

#include <iostream>
#include <string>
#include <deque>
#include <chrono>
using namespace std;

struct flclient_t {

};

void client_connect(flclient_t& self, const string& endpoint);
deque<string> client_request(flclient_t& self, const string& request);

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

		// http://zguide.zeromq.org/page:all#Model-Two-Brutal-Shotgun-Massacre

	}




	return 0;
}

void client_connect(flclient_t& self, const string& endpoint) {

}

deque<string> client_request(flclient_t& self, const string& request) {

}