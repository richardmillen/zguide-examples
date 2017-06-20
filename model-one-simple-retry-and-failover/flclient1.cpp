#include <zmq.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
using namespace std;

#define MAX_RETRIES 3

string try_reqrep(zmq::context_t& context, const string& endpoint, const string& request);

//  Freelance client - Model 1
//  Uses REQ socket to query one or more services

int main(int argc, char* argv[]) {
	vector<string> endpoints;
	po::options_description desc("Freelance client options");
	desc.add_options()
		("help", "prints this help message")
		("endpoints,e", po::value<vector<string>>(&endpoints), "endpoint address (default is localhost:5555)")
		;

	po::variables_map vm;
	po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
	po::notify(vm);

	if (vm.count("help") == 1) {
		cout << "Usage: flclient1 [options]" << endl;
		cout << desc;
		return 0;
	}

	if (endpoints.empty()) {
		cout << "Client will connect to default endpoint..." << endl;

		endpoints.push_back("localhost:5555");
	}

	zmq::context_t context(1);
	string reply;

	if (endpoints.size() == 1) {
		auto endpoint(endpoints.front());

		for (size_t attempt = 0; attempt < MAX_RETRIES; ++attempt) {
			reply = try_reqrep(context, endpoint, "Hello World");
			if (!reply.empty())
				break;

			cout << "No response from " << endpoint << "." << endl;
		}
	}
	else {
		for (auto& endpoint : endpoints) {
			reply = try_reqrep(context, endpoint, "Hello World");
			if (!reply.empty())
				break;

			cout << "No repsonse from " << endpoint << "." << endl;
		}
	}
	
	if (reply.empty()) {
		cout << "There are no servers running." << endl;
	}
	else {
		cout << "Service is running OK: " << reply << "." << endl;
	}

	cout << "Finished. " << endl;

	return 0;
}

string try_reqrep(zmq::context_t& context, const string& endpoint, const string& request) {
	cout << "Trying echo service at " << endpoint << "..." << endl;

	zmq::socket_t client(context, ZMQ_REQ);
	client.connect(string("tcp://" + endpoint));

	// if we don't zero-out the linger value this app won't close properly after failed req-rep attempts.
	auto no_linger = 0;
	client.setsockopt(ZMQ_LINGER, &no_linger, sizeof(no_linger));

	client.send(request.c_str(), request.size());

	zmq::pollitem_t items[] = { client, 0, ZMQ_POLLIN, 0 };
	zmq::poll(items, 1, 1s);

	if ((items[0].revents & ZMQ_POLLIN) == 0)
		return string();

	zmq::message_t reply;
	client.recv(&reply);

	return string(static_cast<char*>(reply.data()), reply.size());
}