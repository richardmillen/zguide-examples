// pathological publisher
// sends out 1,000 topics and then one random update per second.

#include <zmq.hpp>

#include <iostream>
#include <string>
#include <thread>
#include <random>
using namespace std;

bool publish(zmq::socket_t& socket, const size_t& topic_nbr, const string& data);

int main(int argc, char* argv[]) {
	zmq::context_t context(1);
	zmq::socket_t pub(context, ZMQ_PUB);

	string port{ "5556" };
	if (argc == 2)
		port.assign(argv[1]);
	
	cout << "pathopub: binding to port " << port << "..." << endl;
	pub.bind("tcp://*:" + port);
	
	// ensure subscriber connection has time to complete
	this_thread::sleep_for(1s);

	// send out all 1,000 topic messages
	for (size_t topic_nbr = 1; topic_nbr <= 1'000; ++topic_nbr) {
		if (!publish(pub, topic_nbr, "Save Roger!"))
			return 0;
	}

	random_device rd;
	mt19937 eng(rd());
	uniform_int_distribution<> distr(1, 1'000);

	while (true) {
		this_thread::sleep_for(1s);
		if (!publish(pub, distr(eng), "Off with his head!"))
			return 0;
	}

	return 0;
}

bool publish(zmq::socket_t& socket, const size_t& topic_nbr, const string& data) {
	cout << "pathopub: sending [" << topic_nbr << "] " << data << "..." << endl;

	string topic_frame{ to_string(topic_nbr) };
	if (socket.send(topic_frame.c_str(), topic_frame.size(), ZMQ_SNDMORE) == -1)
		return false;
	if (socket.send(data.c_str(), data.size()) == -1)
		return false;
	return true;
}
