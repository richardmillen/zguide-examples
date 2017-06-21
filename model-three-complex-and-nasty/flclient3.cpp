
#include "flcliapi.hpp"

#include <iostream>
#include <chrono>
using namespace std;

#define TOTAL_REQUESTS	1'000

int main(int argc, char* argv[]) {
	flcliapi_t client;

	// connect to several endpoints
	client.connect("tcp://localhost:5555");
	client.connect("tcp://localhost:5556");
	client.connect("tcp://localhost:5557");

	// for pretty numbers
	cout.imbue(locale(""));

	// send a bunch of name resolution 'requests', measure time
	auto requests = TOTAL_REQUESTS;
	auto start = chrono::system_clock::now();
	while (requests) {
		auto reply = client.request("random name");
		if (reply.empty()) {
			cerr << "flclient3: name service not available, aborting..." << endl;
			break;
		}
		--requests;
	}

	auto elapsed = chrono::system_clock::now() - start;
	auto secs = chrono::duration_cast<chrono::seconds>(elapsed);
	auto ms = chrono::duration_cast<chrono::milliseconds>(elapsed);
	auto ns = chrono::duration_cast<chrono::nanoseconds>(elapsed);
	auto reply_count = TOTAL_REQUESTS - requests;

	cout << "flclient3: " << secs.count() << ", received " << reply_count << " replies." << endl;
	cout << "flclient3: Average round trip cost: " <<
		"\t" << ms.count() / reply_count << " ms" << endl <<
		"\t" << ns.count() / reply_count << " ns" << endl;

	return 0;
}