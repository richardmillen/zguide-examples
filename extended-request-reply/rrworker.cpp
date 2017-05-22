#include <zmq.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>
#include <sstream>
using namespace std;

#if defined(WIN32)
#define sleep(n)		Sleep(n)
#endif

int main(int argc, char* argv[]) {
	try {
		string broker_addr;
		int broker_port;

		po::options_description desc("Worker options");
		desc.add_options()
			("help", "prints this help message")
			("broker,b", po::value<string>(&broker_addr)->default_value("localhost"), "address of broker to connect to")
			("port,p", po::value<int>(&broker_port)->default_value(5560), "port number of broker to connect to")
			;

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
		po::notify(vm);

		if (vm.count("help")) {
			cout << "Usage: rrworker [options]\n";
			cout << desc;
			return 0;
		}

		cout << "worker: Connecting to broker '" << broker_addr << "', on port " << broker_port << "..." << endl;

		ostringstream oss;
		oss << "tcp://" << broker_addr << ":" << broker_port;

		zmq::context_t context(1);

		zmq::socket_t responder(context, ZMQ_REP);
		responder.connect(oss.str());

		while (true) {
			zmq::message_t request;
			responder.recv(&request);
			string str(static_cast<char*>(request.data()), request.size());

			cout << "worker: Received request: " << str << endl;

			sleep(1);

			zmq::message_t reply(5);
			memcpy(reply.data(), "World", 5);
			responder.send(reply);
		}
	}
	catch (exception& e) {
		cout << e.what() << endl;
		return 1;
	}
	return 0;
}
