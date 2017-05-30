#include <zmq.hpp>

#include <iostream>
#include <random>
#include <thread>
#include <chrono>
using namespace std;

int main(int argc, char* argv[]) {
	random_device rd;
	mt19937 eng(rd());
	uniform_int_distribution<> crash_distr(0, 29);
	uniform_int_distribution<> slow_distr(0, 9);

	zmq::context_t context(1);
	zmq::socket_t server(context, ZMQ_REP);
	server.bind("tcp://*:5555");

	auto cycles = 0;
	while (true) {
		zmq::message_t request;
		server.recv(&request);
		string req_str(static_cast<char*>(request.data()), request.size());

		++cycles;

		// simulate various problems, after a few cycles
		if (cycles > 3 && crash_distr(eng) == 0) {
			cout << "lpserver: Simulating a crash..." << endl;
			break;
		}
		else if (cycles > 3 && slow_distr(eng) == 0) {
			cout << "lpserver: Simulating CPU overload..." << endl;
			this_thread::sleep_for(2s);
		}

		cout << "lpserver: Normal request (" << req_str << "). Simulating work..." << endl;
		this_thread::sleep_for(1s);

		cout << "lpserver: Sending reply..." << endl;

		auto& rep_str(req_str);
		server.send(rep_str.c_str(), rep_str.size());
	}

	cout << "lpserver: Shutting down." << endl;

	return 0;
}