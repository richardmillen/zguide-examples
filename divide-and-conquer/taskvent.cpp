#include <zmq.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>
#include <sstream>
#include <random>
#include <algorithm>
using namespace std;

#if defined(WIN32)
#define sleep(n)		Sleep(n)
#endif

int main(int argc, char* argv[]) {
	try {
		ostringstream os;
		int port;
		string sink_addr;
		int sink_port;

		po::options_description desc("Ventilator options");
		desc.add_options()
			("help", "print this help message")
			("port", po::value<int>(&port)->default_value(5557), "port number to bind to")
			("sink-addr", po::value<string>(&sink_addr)->default_value("localhost"), "address of machine running the sink application")
			("sink-port", po::value<int>(&sink_port)->default_value(5558), "port number to connect to sink application on")
			;

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
		po::notify(vm);

		if (vm.count("help")) {
			cout << "Usage: taskvent [options]\n";
			cout << desc;
			return 0;
		}

		zmq::context_t context(1);
		zmq::socket_t sender(context, ZMQ_PUSH);
		os << "tcp://*:" << port;
		sender.bind(os.str());

		cout << "vent: Press enter when the workers are ready: " << endl;
		getchar();

		cout << "vent: Sending tasks to workers...\n" << endl;

		os.str(string());

		zmq::socket_t sink(context, ZMQ_PUSH);
		os << "tcp://" << sink_addr << ":" << sink_port;
		sink.connect(os.str());
		zmq::message_t message(2);
		memcpy(message.data(), "0", 1);
		sink.send(message);

		random_device rd;
		mt19937 eng(rd());
		uniform_int_distribution<> distr(1, 100);

		auto total_ms = 0u;
		for (size_t task_nbr = 0; task_nbr < 100; ++task_nbr) {
			auto workload = distr(eng);
			total_ms += workload;

			message.rebuild(10);
			memset(message.data(), 0, 10);
			sprintf((char*)message.data(), "%d", workload);
			sender.send(message);
		}

		cout << "vent: Total expected cost: " << total_ms << "ms" << endl;
		sleep(1);
	}
	catch (exception& e) {
		cout << "vent: " << e.what() << endl;
		return 1;
	}
	return 0;
}

