#include <zmq.hpp>

#include <boost/asio/ip/host_name.hpp>
namespace ip = boost::asio::ip;

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <random>
using namespace std;

int main(int argc, char* argv[]) {
	try {
		ostringstream oss;
		int cloudfe_port;
		vector<string> peers;
		int localfe_port;
		int localbe_port;
		int client_count;
		int worker_count;
		po::options_description desc("Options");
		desc.add_options()
			("help", "prints this help message")
			("cloud-port", po::value<int>(&cloudfe_port)->required(), "port number for cloud peers (other brokers) to connect to us on")
			("peer", po::value<vector<string>>(&peers)->required(), "<address:port> of another cloud broker to connect to")
			("frontend-port", po::value<int>(&localfe_port)->required(), "local frontend port number to receive requests on")
			("backend-port", po::value<int>(&localbe_port)->required(), "local backend port number for workers to connec to us on")
			("clients", po::value<int>(&client_count)->default_value(10), "number of clients to start")
			("workers", po::value<int>(&worker_count)->default_value(3), "number of workers to start")
			;

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
		
		if (vm.count("help")) {
			cout << "Usage: peering2 [options]\n" << endl;
			cout << desc << endl;
			return 0;
		}

		po::notify(vm);

		auto fe_id("localhost:" + to_string(cloudfe_port));
		auto be_id(fe_id);

		zmq::context_t context(1);

		zmq::socket_t cloudfe(context, ZMQ_ROUTER);
		cloudfe.setsockopt(ZMQ_IDENTITY, fe_id.c_str(), fe_id.size());
		cloudfe.bind("tcp://*:" + to_string(cloudfe_port));

		zmq::socket_t cloudbe(context, ZMQ_ROUTER);
		cloudbe.setsockopt(ZMQ_IDENTITY, be_id.c_str(), be_id.size());
		
		for (const auto& peer : peers) {
			cloudbe.connect("tcp://" + peer);

			cout << "peering2: Connected to " << peer << "." << endl;
		}

		zmq::socket_t localfe(context, ZMQ_ROUTER);
		localfe.bind("tcp://*:" + to_string(localfe_port));

		zmq::socket_t localbe(context, ZMQ_ROUTER);
		localbe.bind("tcp://*:" + to_string(localbe_port));

		cout << "Press enter when all brokers are started: ";
		getchar();




		// http://zguide.zeromq.org/page:all#Prototyping-the-Local-and-Cloud-Flows
	}
	catch (exception& e) {
		cerr << e.what() << endl;
		return 1;
	}
	return 0;
}

