#include <zmq.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>
#include <string>
using namespace std;

int main(int argc, char* argv[]) {
	int port;
	po::options_description desc("Freelance server options");
	desc.add_options()
		("help", "prints this help text")
		("port,p", po::value<int>(&port)->default_value(5555), "port number to bind to")
		;

	po::variables_map vm;
	po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
	po::notify(vm);

	if (vm.count("help") == 1) {
		cout << "Usage: flserver1 [options]" << endl;
		cout << desc;
		return 0;
	}

	zmq::context_t context(1);
	zmq::socket_t server(context, ZMQ_REP);
	server.bind(string("tcp://*:" + to_string(port)));

	cout << "Listening on port " << port << "..." << endl;

	while (true) {
		zmq::message_t request;
		if (!server.recv(&request)) {
			cout << "Interrupted." << endl;
			break;
		}

		zmq::message_t reply;
		reply.copy(&request);
		server.send(reply);
	}
	
	return 0;
}
