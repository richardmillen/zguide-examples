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
		ostringstream os;
		string vent_addr;
		int vent_port;
		string sink_addr;
		int sink_port;
		int ctrl_port;

		po::options_description desc("Worker options");
		desc.add_options()
			("help", "prints this help message")
			("vent-addr", po::value<string>(&vent_addr)->default_value("localhost"), "address of ventilator app")
			("vent-port", po::value<int>(&vent_port)->default_value(5557), "port number of ventilator app")
			("sink-addr", po::value<string>(&sink_addr)->default_value("localhost"), "address of sink app")
			("sink-port", po::value<int>(&sink_port)->default_value(5558), "port number of sink app")
			("ctrl-port", po::value<int>(&ctrl_port)->default_value(5559), "port number on which to subscribe to control messages")
			;

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
		po::notify(vm);

		if (vm.count("help")) {
			cout << "Usage: taskwork [options]\n";
			cout << desc;
			return 0;
		}

		zmq::context_t context(1);

		cout << "work: Connecting to ventilator '" << vent_addr << "', on port " << vent_port << "..." << endl;

		zmq::socket_t receiver(context, ZMQ_PULL);
		os << "tcp://" << vent_addr << ":" << vent_port;
		receiver.connect(os.str());

		cout << "work: Connecting to sink '" << sink_addr << "', on port " << sink_port << "..." << endl;

		zmq::socket_t sender(context, ZMQ_PUSH);
		os.str(string());
		os << "tcp://" << sink_addr << ":" << sink_port;
		sender.connect(os.str());

		cout << "work: Connecting to sink control port '" << ctrl_port << "..." << endl;

		zmq::socket_t controller(context, ZMQ_SUB);
		os.str(string());
		os << "tcp://" << sink_addr << ":" << ctrl_port;
		controller.connect(os.str());
		controller.setsockopt(ZMQ_SUBSCRIBE, "", 0);

		zmq::pollitem_t items[] = {
			{receiver, 0, ZMQ_POLLIN, 0},
			{controller, 0, ZMQ_POLLIN, 0}
		};

		cout << "work: Connected. Waiting for work..." << endl;

		while (true) {
			zmq::poll(items, 2, -1);

			if (items[0].revents & ZMQ_POLLIN) {
				zmq::message_t message;
				int workload;

				receiver.recv(&message);

				string str(static_cast<char*>(message.data()), message.size());
				istringstream is(str);
				is >> workload;

				sleep(workload);

				message.rebuild();
				sender.send(message);

				cout << "." << flush;
			}

			if (items[1].revents & ZMQ_POLLIN) {
				cout << "\nwork: Terminating..." << endl;
				break;
			}
		}
	}
	catch (exception& e) {
		cout << e.what() << endl;
		return 1;
	}
	return 0;
}

