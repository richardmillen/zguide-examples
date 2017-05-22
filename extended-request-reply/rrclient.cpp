#include <zmq.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>
#include <sstream>
using namespace std;

int main(int argc, char* argv[]) {
	try {
		string server_addr;
		int server_port;
		int req_count;

		po::options_description desc("Client options");
		desc.add_options()
			("help", "prints this message")
			("server,s", po::value<string>(&server_addr)->default_value("localhost"), "address of the server to send requests to")
			("port,p", po::value<int>(&server_port)->default_value(5559), "port number to send requests to")
			("requests,r", po::value<int>(&req_count)->default_value(100), "number of requests to send")
			;

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
		po::notify(vm);

		if (vm.count("help")) {
			cout << "Usage: rrclient [options]\n";
			cout << desc;
			return 0;
		}

		cout << "client: Connecting to server '" << server_addr << "', on port " << server_port << "..." << endl;

		ostringstream oss;
		oss << "tcp://" << server_addr << ":" << server_port;

		zmq::context_t context(1);

		zmq::socket_t requester(context, ZMQ_REQ);
		requester.connect(oss.str());

		for (size_t req_no = 0; req_no < req_count; ++req_no) {
			zmq::message_t request(5);
			memcpy(request.data(), "Hello", 5);
			requester.send(request);
			
			zmq::message_t reply;
			requester.recv(&reply);
			string str(static_cast<char*>(reply.data()), reply.size());

			cout << "client: Received reply " << req_no << " [" << str << "]" << endl;
		}
	}
	catch (exception& e) {
		cout << e.what() << endl;
		return 1;
	}
	return 0;
}
