#include <zmq.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>
#include <sstream>
#include <chrono>
using namespace std;

int main(int argc, char* argv[]) {
	try {
		ostringstream oss;
		int recv_port;
		int ctrl_port;
		po::options_description desc("Sink options");
		desc.add_options()
			("help", "print this help message")
			("port", po::value<int>(&recv_port)->default_value(5558), "port number to bind to")
			("ctrl-port", po::value<int>(&ctrl_port)->default_value(5559), "port number to send control messages from")
			;

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
		po::notify(vm);

		if (vm.count("help")) {
			cout << "Usage: tasksink [options]\n";
			cout << desc;
			return 0;
		}

		cout << "sink: Binding to receive port " << recv_port << ", and control port " << ctrl_port << "..." << endl;

		zmq::context_t context(1);

		zmq::socket_t receiver(context, ZMQ_PULL);
		oss << "tcp://*:" << recv_port;
		receiver.bind(oss.str());

		zmq::socket_t controller(context, ZMQ_PUB);
		oss.str(string());
		oss << "tcp://*:" << ctrl_port;
		controller.bind(oss.str());

		cout << "sink: Waiting for the go ahead from the ventilator..." << endl;

		zmq::message_t message;
		receiver.recv(&message);

		cout << "sink: Ready to receive results." << endl;

		auto start = chrono::system_clock::now();

		auto total_ms = 0u;
		for (size_t task_nbr = 0; task_nbr < 100; ++task_nbr) {
			receiver.recv(&message);
			if ((task_nbr / 10) * 10 == task_nbr)
				cout << ":" << flush;
			else
				cout << "." << flush;
		}

		auto end = chrono::system_clock::now();

		chrono::duration<double> elapsed_secs = end - start;
		time_t end_time = chrono::system_clock::to_time_t(end);

		cout << "\nsink: Finished at " << ctime(&end_time)
			<< "elapsed time: " << elapsed_secs.count() << "s" << endl;

		cout << "sink: Sending 'KILL' message to workers..." << endl;

		message.rebuild(4);
		memcpy(message.data(), "KILL", 4);
		controller.send(message);

		cout << "sink: Shutting down..." << endl;
	}
	catch (exception& e) {
		cout << e.what() << endl;
		return 1;
	}
	return 0;
}

