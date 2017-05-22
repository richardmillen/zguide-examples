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
		string pub_addr;
		int pub_port;
		string pub_filter;

		po::options_description desc("Reader options");
		desc.add_options()
			("help", "print this help message")
			("vent-addr", po::value<string>(&vent_addr)->default_value("localhost"), "ventilator address")
			("vent-port", po::value<int>(&vent_port)->default_value(5557), "ventilator port number")
			("pub-addr", po::value<string>(&pub_addr)->default_value("localhost"), "publisher address")
			("pub-port", po::value<int>(&pub_port)->default_value(5556), "publisher port number")
			("pub-filter", po::value<string>(&pub_filter)->default_value("001"), "3 digit (zero padded) subscription filter")
			;

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
		po::notify(vm);

		if (vm.count("help")) {
			cout << "Usage msreader [options]\n";
			cout << desc;
			return 0;
		}

		zmq::context_t context(1);

		zmq::socket_t receiver(context, ZMQ_PULL);
		os << "tcp://" << vent_addr << ":" << vent_port;
		receiver.connect(os.str());

		os.str(string());

		zmq::socket_t subscriber(context, ZMQ_SUB);
		os << "tcp://" << pub_addr << ":" << pub_port;
		subscriber.connect(os.str());
		subscriber.setsockopt(ZMQ_SUBSCRIBE, pub_filter.c_str(), 3);

		while (true) {
			bool rc;
			do {
				zmq::message_t task;
				if ((rc == receiver.recv(&task, ZMQ_DONTWAIT)) == true) {
					// process task
				}
			} while (rc == true);

			do {
				zmq::message_t update;
				if ((rc == subscriber.recv(&update, ZMQ_DONTWAIT)) == true) {
					// process update
				}
			} while (rc == true);

			sleep(1);
		}
	}
	catch (exception& e) {
		cout << e.what() << endl;
		return 1;
	}
	return 0;
}
