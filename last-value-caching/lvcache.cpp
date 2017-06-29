// last value cache
// uses XPUB subscription messages to re-send data
// 
// run the proxy, and then the publisher:
// ./lvcache &
// ./pathopub 5557
// 
// And now run as many instances of the subscriber as you want to try
// ./pathosub localhost:5558

#include <zmq.hpp>

#include <iostream>
#include <string>
#include <array>
#include <unordered_map>
using namespace std;

int main(int argc, char* argv[]) {
	zmq::context_t context(1);

	zmq::socket_t frontend(context, ZMQ_SUB);
	frontend.connect("tcp://localhost:5557");
	frontend.setsockopt(ZMQ_SUBSCRIBE, "", 0);

	zmq::socket_t backend(context, ZMQ_XPUB);
	backend.bind("tcp://*:5558");

	cout << "lvcache: subscribing to all topics from localhost:5557, publishing on port 5558..." << endl;

	// store last instance of each topic in cache
	unordered_map<string, string> cache;

	// we route topic updates from frontend to backend, and
	// we handle subscriptions by sending whatever we cached,
	// if anything
	while (true) {
		zmq::pollitem_t items[] = {
			{ frontend, 0, ZMQ_POLLIN, 0 },
			{ backend,  0, ZMQ_POLLIN, 0 }
		};
		if (zmq::poll(items, 2, 1s) == -1)
			break;			// interrupted

		// any new topic data we cache and then forward
		if (items[0].revents & ZMQ_POLLIN) {
			zmq::message_t topic_frame;
			frontend.recv(&topic_frame);
			string topic{ static_cast<char*>(topic_frame.data()), topic_frame.size() };

			zmq::message_t data_frame;
			frontend.recv(&data_frame);
			string data{ static_cast<char*>(data_frame.data()), data_frame.size() };

			cache[topic] = data;

			backend.send(topic.c_str(), topic.size(), ZMQ_SNDMORE);
			backend.send(data.c_str(), data.size());
		}
		// when we get a new subscription, we pull data from the cache
		if (items[1].revents & ZMQ_POLLIN) {
			zmq::message_t frame;
			if (!backend.recv(&frame))
				break;		// interrupted

			// event is one byte 0=unsub or 1=sub, followed by topic
			char sub_data[8] = {};
			memcpy(sub_data, frame.data(), frame.size());

			if (sub_data[0]) {
				string cached_topic{ sub_data + 1 };
				if (cache.count(cached_topic)) {
					cout << "lvcache: sending cached topic " << cached_topic << "..." << endl;

					auto data{ cache[cached_topic] };
					
					backend.send(cached_topic.c_str(), cached_topic.size(), ZMQ_SNDMORE);
					backend.send(data.c_str(), data.size());
				}
				else {
					cout << "lvcache: no last cached value for new subscriber." << endl;
				}
			}
		}
	}

	return 0;
}
