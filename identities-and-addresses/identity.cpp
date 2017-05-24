#include <zmq.hpp>

#include <iostream>
#include <iomanip>
#include <string>
using namespace std;

// print_parts is called to receive all the parts of a message, printing each one to the console.
void print_parts(zmq::socket_t& socket) {
	auto more_parts = TRUE;
	
	while (more_parts) {
		zmq::message_t req_part;
		socket.recv(&req_part);

		string req(static_cast<char*>(req_part.data()), req_part.size());

		auto is_text = true;
		for (auto ch : req) {
			if (ch < 32 || ch > 127) {
				is_text = false;
				break;
			}
		}

		cout << "[" << setfill('0') << setw(3) << req_part.size() << "] ";
		for (auto ch : req) {
			if (is_text)
				cout << ch;
			else
				cout << setfill('0') << setw(2) << hex << static_cast<unsigned int>(ch) << " ";
		}
		cout << endl;

		more_parts = 0;
		auto more_size = sizeof(more_parts);
		socket.getsockopt(ZMQ_RCVMORE, &more_parts, &more_size);
	}
}

int main(int argc, char* argv[]) {
	zmq::context_t context(1);
	zmq::socket_t sink(context, ZMQ_ROUTER);
	sink.bind("inproc://example");

	zmq::socket_t anonymous(context, ZMQ_REQ);
	anonymous.connect("inproc://example");

	string anon_str("ROUTER uses a generated 5 byte identity");
	zmq::message_t anon_msg(anon_str.size());
	memcpy(anon_msg.data(), anon_str.c_str(), anon_str.size());
	anonymous.send(anon_msg);

	print_parts(sink);

	zmq::socket_t identified(context, ZMQ_REQ);
	identified.setsockopt(ZMQ_IDENTITY, "PEER2", 5);
	identified.connect("inproc://example");

	string id_str("ROUTER socket uses REQ's socket identity");
	zmq::message_t id_msg(id_str.size());
	memcpy(id_msg.data(), id_str.c_str(), id_str.size());
	identified.send(id_msg);

	print_parts(sink);

	cout << "That's all folks!" << endl;
	getchar();

	return 0;
}






