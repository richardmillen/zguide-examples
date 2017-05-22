#include <zmq.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>
#include <sstream>
using namespace std;

int main(int argc, char* argv[]) {
	try {
		ostringstream oss;
		int front_port;
		int back_port;
		po::options_description desc("Broker options");
		desc.add_options()
			("help", "prints this message")
			("frontend-port", po::value<int>(&front_port)->default_value(5559), "port number for clients to connect to")
			("backend-port", po::value<int>(&back_port)->default_value(5560), "port number for workers to connect to")
			;

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
		po::notify(vm);

		if (vm.count("help")) {
			cout << "Usage: rrbroker [options]\n";
			cout << desc;
			return 0;
		}

		zmq::context_t context(1);

		zmq::socket_t frontend(context, ZMQ_ROUTER);
		zmq::socket_t backend(context, ZMQ_DEALER);

		oss << "tcp://*:" << front_port;
		frontend.bind(oss.str());

		oss.str(string());
		oss << "tcp://*:" << back_port;
		backend.bind(oss.str());

		cout << "broker: Listening for requests on port " << front_port << "..." << endl;
		cout << "broker: Allowing workers to connect on port " << back_port << "..." << endl;

		zmq::pollitem_t items[] = {
			{ frontend, 0, ZMQ_POLLIN, 0 },
			{ backend, 0, ZMQ_POLLIN, 0 }
		};

		while (true) {
			zmq::message_t message;
			int more_parts;
			
			zmq::poll(&items[0], 2, -1);

			if (items[0].revents & ZMQ_POLLIN) {
				while (true) {
					frontend.recv(&message);
					auto more_size = sizeof(more_parts);
					frontend.getsockopt(ZMQ_RCVMORE, &more_parts, &more_size);
					backend.send(message, more_parts ? ZMQ_SNDMORE : 0);

					if (!more_parts)
						break;
				}
			}

			if (items[1].revents & ZMQ_POLLIN) {
				while (true) {
					backend.recv(&message);
					auto more_size = sizeof(more_parts);
					backend.getsockopt(ZMQ_RCVMORE, &more_parts, &more_size);
					frontend.send(message, more_parts ? ZMQ_SNDMORE : 0);

					if (!more_parts)
						break;
				}
			}
		}
	}
	catch (exception& e) {
		cout << e.what() << endl;
		return 1;
	}
	return 0;
}
