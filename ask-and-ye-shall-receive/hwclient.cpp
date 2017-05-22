#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <zmq.hpp>
#include <string>
#include <iostream>
#include <sstream>

int main(int argc, char* argv[]) {
	try {
		std::string addr;
		int portno;

		po::options_description desc("Client options");
		desc.add_options()
			("help", "print this help message")
			("server,s", po::value<std::string>(&addr)->default_value("localhost"), "address of the server to connect to")
			("port,p", po::value<int>(&portno)->default_value(5555), "port number on the server to connect to")
			;

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
		po::notify(vm);

		if (vm.count("help")) {
			std::cout << "Usage: hwclient [options]\n";
			std::cout << desc;
			return 0;
		}

		zmq::context_t context(1);
		zmq::socket_t socket(context, ZMQ_REQ);

		std::ostringstream os;
		os << "tcp://" << addr << ":" << portno;

		std::cout << "client: Connecting to server '" << addr << "' on port " << portno << "..." << std::endl;
		socket.connect(os.str());

		for (int request_nbr = 0; request_nbr < 10; ++request_nbr) {
			zmq::message_t request(5);
			memcpy(request.data(), "Hello", 5);
			std::cout << "client: Sending Hello " << request_nbr << "..." << std::endl;
			socket.send(request);

			zmq::message_t reply;
			socket.recv(&reply);
			std::cout << "client: Received Word " << request_nbr << std::endl;
		}
	}
	catch (std::exception& e) {
		std::cout << "client: " << e.what() << "\n";
		return 1;
	}
	return 0;
}

