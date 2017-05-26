#include <zmq.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <boost/asio/ip/host_name.hpp>
namespace ip = boost::asio::ip;

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <random>
using namespace std;

int main(int argc, char* argv[]) {
	try {
		ostringstream oss;
		int state_port;
		vector<string> peers;
		int peer_port;
		po::options_description desc("Peering1 options");
		desc.add_options()
			("help", "print this help message")
			("state-port", po::value<int>(&state_port)->required(), "the port that state updates will be published on")
			("peer-addr", po::value<vector<string>>(&peers)->required(), "the addresses (excluding port) of each peer broker")
			("peer-port", po::value<int>(&peer_port)->required(), "the port number used by all peer brokers to publish state updates on")
			;

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
		
		if (vm.count("help")) {
			cout << "Usage: peering1 [options]" << endl;
			cout << desc << endl;
			return 0;
		}
		
		po::notify(vm);

		zmq::context_t context(1);
		
		zmq::socket_t statebe(context, ZMQ_PUB);
		oss << "tcp://*:" << state_port;
		statebe.bind(oss.str());

		zmq::socket_t statefe(context, ZMQ_SUB);
		statefe.setsockopt(ZMQ_SUBSCRIBE, "", 0);

		auto remote_peers = false;

		for (const auto& peer : peers) {
			oss.str(string());
			oss << "tcp://" << peer << ":" << peer_port;
			statefe.connect(oss.str());

			cout << "peering1: Connected to " << oss.str() << "." << endl;

			if (peer.compare("localhost") != 0)
				remote_peers = true;
		}

		string self("localhost");
		if (remote_peers)
			self.assign(ip::host_name());
		self.append(":");
		self.append(to_string(state_port));

		cout << "peering1 (" << self << "): Broker running..." << endl;

		random_device rd;
		mt19937 eng(rd());
		uniform_int_distribution<> distr(1, 10);

		while (true) {
			zmq::pollitem_t items[] = { statefe, 0, ZMQ_POLLIN, 0 };

			if (zmq::poll(items, 1, chrono::seconds(1)) == -1)
				break;

			if (items[0].revents & ZMQ_POLLIN) {
				zmq::message_t message;

				statefe.recv(&message);
				string peer_name(static_cast<char*>(message.data()), message.size());

				message.rebuild();
				statefe.recv(&message);
				string avail(static_cast<char*>(message.data()), message.size());

				cout << "peering1: (" << self << "): " << peer_name << " - " << avail << " workers available." << endl;
			}
			else {
				statebe.send(self.c_str(), self.size(), ZMQ_SNDMORE);
				
				auto fake_avail(to_string(distr(eng)));
				statebe.send(fake_avail.c_str(), fake_avail.size());
			}
		}
	}
	catch (exception& e) {
		cerr << e.what() << endl;
		return 1;
	}
	return 0;
}

