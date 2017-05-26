#include <zmq.hpp>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <random>
#include <mutex>
using namespace std;

#if defined(WIN32)
#define sleep(n)		Sleep(n)
#endif

mutex mut;

void client_task();
void server_task();
void server_work(zmq::context_t* context, size_t num);
string random_id();
void print(const string& who, const string& text);
void print(const string& who, const string& text, zmq::message_t& message);
void print(const string& who, const string& id, const string& text);
void print(const string& who, const string& id, zmq::socket_t& socket);

int main(int argc, char* argv[]) {
	thread c1(client_task);
	thread c2(client_task);
	thread c3(client_task);
	thread svr(server_task);

	print("main", "Waiting for peers to complete...");

	c1.join();
	c2.join();
	c3.join();
	svr.join();

	print("main", "Done!");
	getchar();

	return 0;
}

void client_task() {
	auto id(random_id());

	zmq::context_t context(1);
	zmq::socket_t client(context, zmq::socket_type::dealer);
	client.setsockopt(ZMQ_IDENTITY, id.c_str(), id.size());
	client.connect("tcp://localhost:5570");

	print("Client", id, "Connected.");

	zmq::pollitem_t items[] = { client, 0, ZMQ_POLLIN, 0 };

	auto request_nbr = 0;
	while (true) {
		for (size_t i = 0; i < 100; ++i) {
			zmq::poll(items, 1, chrono::milliseconds(10));

			if (items[0].revents & ZMQ_POLLIN)
				print("Client", id, client);
		}

		char request[16] = {};
		sprintf(request, "request #%d", ++request_nbr);
		client.send(request, strlen(request));
	}
}

void server_task() {
	zmq::context_t context(1);
	
	zmq::socket_t frontend(context, zmq::socket_type::router);
	frontend.bind("tcp://*:5570");

	zmq::socket_t backend(context, zmq::socket_type::dealer);
	backend.bind("inproc://backend");

	print("Server", "Starting workers...");

	vector<thread> workers;
	for (size_t worker_nbr = 0; worker_nbr < 5; ++worker_nbr) {
		workers.push_back(thread(server_work, &context, worker_nbr));
	}

	print("Server", "Running proxy...");

	zmq::proxy(frontend, backend, nullptr);

	for (auto& worker : workers)
		worker.join();

	print("Server", "Finished.");
}

void server_work(zmq::context_t* context, size_t num) {
	auto id(to_string(num));

	zmq::socket_t worker(*context, zmq::socket_type::dealer);
	worker.connect("inproc://backend");

	print("Worker", id, "Connected to backend.");

	random_device rd;
	mt19937 eng(rd());
	uniform_int_distribution<> rep_distr(1, 5);
	uniform_int_distribution<> sleep_distr(1, 1000);

	while (true) {
		zmq::message_t id_part;
		zmq::message_t msg_part;
		zmq::message_t copied_id;
		zmq::message_t copied_msg;
		
		worker.recv(&id_part);
		worker.recv(&msg_part);

		auto replies = rep_distr(eng);
		for (size_t i = 0; i < replies; ++i) {
			sleep(sleep_distr(eng));

			copied_id.copy(&id_part);
			copied_msg.copy(&msg_part);

			print("Worker", "Copied id", copied_id);
			print("Worker", "Copied message", copied_msg);

			worker.send(copied_id, ZMQ_SNDMORE);
			worker.send(copied_msg);
		}
	}
}

string random_id() {
	random_device rd;
	mt19937 eng(rd());
	uniform_int_distribution<> distr(0x00000, 0x10000);

	char identity[10] = {};
	sprintf(identity, "%04X-%04X", distr(eng), distr(eng));

	return string(identity);
}

void print(const string& who, const string& text) {
	unique_lock<decltype(mut)> lock(mut);
	cout << "asyncsrv: " << who << ": " << text << endl;
}

void print(const string& who, const string& text, zmq::message_t& message) {
	unique_lock<decltype(mut)> lock(mut);

	string str(static_cast<char*>(message.data()), message.size());

	cout << "asyncsrv: " << who << ": " << text << " - " << str << endl;
}

void print(const string& who, const string& id, const string& text) {
	unique_lock<decltype(mut)> lock(mut);

	cout << "asyncsrv: " << who << " (" << id << "): " << text << endl;
}

void print(const string& who, const string& id, zmq::socket_t& socket) {
	unique_lock<decltype(mut)> lock(mut);

	cout << "asyncsrv: " << who << " (" << id << "): Dumping..." << endl;

	auto more_parts = TRUE;
	
	while (more_parts) {
		zmq::message_t message;
		socket.recv(&message);

		string data(static_cast<char*>(message.data()), message.size());

		auto is_text = true;
		for (auto ch : data) {
			if (ch < 32 || ch > 127) {
				is_text = false;
				break;
			}
		}

		cout << "asyncsrv: " << who << " (" << id << "): " << "[" << setfill('0') << setw(3) << data.size() << "] ";
		for (auto ch : data) {
			if (is_text)
				cout << ch;
			else
				cout << setfill('0') << setw(2) << hex << static_cast<unsigned int>(ch);
		}
		cout << endl;

		more_parts = 0;
		auto more_size = sizeof(more_parts);

		socket.getsockopt(ZMQ_RCVMORE, &more_parts, &more_size);
	}
}






