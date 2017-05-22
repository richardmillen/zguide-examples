#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <zmq.hpp>
#include <string>
#include <iostream>
#include <sstream>

#if defined (WIN32)
#include <windows.h>
#define sleep(n)	Sleep(n)
#else
#include <unistd.h>
#endif

int main(int argc, char* argv[]) {
	try {
		std::string addr;
		int portno;

		po::options_description desc("Server options");
		desc.add_options()
			("help", "print this help message")
			("address,a", po::value<std::string>(&addr)->default_value("*"), "local address to bind to")
			("port,p", po::value<int>(&portno)->default_value(5555), "port to listen on")
			;

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
		po::notify(vm);

		if (vm.count("help")) {
			std::cout << "Usage: hwserver [options]\n";
			std::cout << desc;
			return 0;
		}

		std::ostringstream os;
		os << "tcp://" << addr << ":" << portno;

		zmq::context_t context(1);
		zmq::socket_t socket(context, ZMQ_REP);
		socket.bind(os.str());

		std::cout << "server: Listening on port " << portno << "..." << std::endl;

		while (true) {
			zmq::message_t request;

			socket.recv(&request);
			std::cout << "server: Received Hello" << std::endl;

			sleep(1);

			zmq::message_t reply(5);
			memcpy(reply.data(), "World", 5);
			socket.send(reply);
		}
	}
	catch (std::exception& e) {
		std::cout << "server: " << e.what() << "\n";
		return 1;
	}
	return 0;
}



