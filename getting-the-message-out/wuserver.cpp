#include <zmq.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>
#include <sstream>
#include <random>
using namespace std;

int main(int argc, char* argv[]) {
	try {
		int portno;
		po::options_description desc("Server options");
		desc.add_options()
			("help", "print this help message")
			("port", po::value<int>(&portno)->default_value(5556), "port number to publish fake weather data on")
			;

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
		po::notify(vm);

		if (vm.count("help")) {
			cout << "Usage: wuserver [options]\n";
			cout << desc;
			return 0;
		}

		ostringstream os;
		os << "tcp://*:" << portno;

		zmq::context_t context(1);
		zmq::socket_t publisher(context, ZMQ_PUB);
		publisher.bind(os.str());
		//publisher.bind("ipc://weather.ipc"); <------ doesn't work on windows

		random_device rd;
		mt19937 eng(rd());
		uniform_int_distribution<> code_distr(1, 999);
		uniform_int_distribution<> temp_distr(-5, 90);
		uniform_int_distribution<> hum_distr(0, 100);

		cout << "server: Sending data to subscribers...";

		while (true) {
			int code, temperature, relhumidity;

			code = code_distr(eng);
			temperature = temp_distr(eng);
			relhumidity = hum_distr(eng);

			zmq::message_t message(20);
			snprintf((char*)message.data(), 20, "%03d %d %d", code, temperature, relhumidity);
			publisher.send(message);
		}
	}
	catch (exception& e) {
		cout << e.what() << "\n";
		return 1;
	}
	return 0;
}

