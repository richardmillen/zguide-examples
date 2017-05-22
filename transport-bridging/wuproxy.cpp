#include <zmq.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>
#include <sstream>
using namespace std;

int main(int argc, char* argv[]) {
	try {
		ostringstream oss;
		string sub_addr;
		int sub_port;
		string pub_addr;
		int pub_port;

		po::options_description desc("Proxy options");
		desc.add_options()
			("help", "prints this help message")
			("sub-addr", po::value<string>(&sub_addr)->required(), "address of publisher")
			("sub-port", po::value<int>(&sub_port)->default_value(5556), "port on publisher")
			("pub-addr", po::value<string>(&pub_addr)->required(), "address subscribers will connect to")
			("pub-port", po::value<int>(&pub_port)->default_value(8100), "port number subscribers will connect to")
			;

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);

		if (vm.count("help")) {
			cout << "Usage: wuproxy [options]" << endl;
			cout << desc;
			return 0;
		}

		// catches any missing 'required' options, but after we've checked the 'help' option.
		po::notify(vm);

		zmq::context_t context(1);

		zmq::socket_t frontend(context, ZMQ_SUB);
		oss << "tcp://" << sub_addr << ":" << sub_port;
		frontend.connect(oss.str());

		oss.str(string());

		zmq::socket_t backend(context, ZMQ_PUB);
		oss << "tcp://" << pub_addr << ":" << pub_port;
		backend.bind(oss.str());

		// subscribe to everything
		frontend.setsockopt(ZMQ_SUBSCRIBE, "", 0);

		while (true) {
			while (true) {
				zmq::message_t message;
				int more_parts;
				auto more_size = sizeof(more_parts);

				frontend.recv(&message);
				frontend.getsockopt(ZMQ_RCVMORE, &more_parts, &more_size);
				backend.send(message, more_parts ? ZMQ_SNDMORE : 0);

				if (!more_parts)
					break;
			}
		}
	}
	catch (exception& e) {
		cerr << e.what() << endl;
		return 1;
	}
	return 0;
}
