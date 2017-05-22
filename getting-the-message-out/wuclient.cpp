#include <zmq.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>
#include <string>
#include <sstream>
using namespace std;

int main(int argc, char* argv[]) {
	try {
		string addr;
		int portno;
		string filter;
		po::options_description desc("Client options");
		desc.add_options()
			("help", "")
			("filter", po::value<string>(&filter)->default_value("001"), "3 digit (zero padded) subscription filter")
			("server", po::value<string>(&addr)->default_value("localhost"), "address of weather update server")
			("port", po::value<int>(&portno)->default_value(5556), "port number to connect to")
			;

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
		po::notify(vm);

		if (vm.count("help")) {
			cout << "Usage: wuclient [options]\n";
			cout << desc;
			return 0;
		}

		ostringstream os;
		os << "tcp://" << addr << ":" << portno;

		cout << "client: Collecting updates from weather server '" << addr << "', port #" << portno << "...\n" << endl;

		zmq::context_t context(1);
		zmq::socket_t subscriber(context, ZMQ_SUB);
		subscriber.connect(os.str());
		subscriber.setsockopt(ZMQ_SUBSCRIBE, filter.c_str(), filter.length());

		int update_nbr;
		long total_temp = 0;
		for (update_nbr = 0; update_nbr < 100; update_nbr++) {
			zmq::message_t update;
			int code, temperature, relhumidity;

			cout << "client: Waiting for data...\n";

			subscriber.recv(&update);

			cout << "client: Got some!\n";

			istringstream iss(static_cast<char*>(update.data()));
			iss >> code >> temperature >> relhumidity;

			total_temp += temperature;
		}

		cout << "client: Average temperature for code '" << filter << "' was " << (total_temp / update_nbr) << "F" << endl;
	}
	catch (exception& e) {
		cerr << e.what() << "\n";
		return 1;
	}
	return 0;
}

