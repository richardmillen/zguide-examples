// pathological subscriber
// subscribes to one random topic and prints received messages
// 
// Try building and running these: first the subscriber, then the publisher.
// You'll see the subscriber reports getting "Save Roger" as you'd expect:
// ./pathosub &
// ./pathopub &
// 
// It's when you run a second subscriber that you understand Roger's predicament. 
// You have to leave it an awful long time before it reports getting any data.
// ./pathosub &
// 
// (continued in lvcache)

#include <zmq.hpp>

#include <iostream>
#include <string>
#include <random>
using namespace std;

#ifdef WIN32
#include <process.h>
#define getpid()		::_getpid()
#else
#define getpid()		::getpid()
#endif

int main(int argc, char* argv[]) {
	zmq::context_t context(1);
	zmq::socket_t sub(context, ZMQ_SUB);

	string endpoint{ "tcp://localhost:5556" };
	if (argc == 2) {
		endpoint.assign("tcp://");
		endpoint.append(argv[1]);
	}
	
	cout << "pathosub (" << getpid() << "): connecting to '" << endpoint << "'..." << endl;
	sub.connect(endpoint);

	random_device rd;
	mt19937 eng(rd());
	uniform_int_distribution<> distr(1, 1'000);

	auto sub_topic{ to_string(distr(eng)) };
	sub.setsockopt(ZMQ_SUBSCRIBE, sub_topic.c_str(), sub_topic.size());

	cout << "pathosub (" << getpid() << "): subscribing to topic '" << sub_topic << "'..." << endl;

	while (true) {
		zmq::message_t topic_frame;
		if (!sub.recv(&topic_frame))
			break;
		zmq::message_t data_frame;
		if (!sub.recv(&data_frame))
			break;

		string topic{ static_cast<char*>(topic_frame.data()), topic_frame.size() };
		//assert(topic == sub_topic);
		if (topic != sub_topic) {
			// occasionally this assertion fails!?

			cerr << "pathosub: bug: '" << topic << "' is not equal to '" << sub_topic << "'." << endl;
			break;
		}

		string data{ static_cast<char*>(data_frame.data()), data_frame.size() };
		cout << "pathosub (" << getpid() << "): " << data << endl;
	}

	return 0;
}
