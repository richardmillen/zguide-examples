#include <zmq.hpp>

#include <iostream>
#include <string>
#include <deque>
#include <sstream>
#include <thread>
#include <mutex>
#include <chrono>
using namespace std;

void run_sync_test(zmq::socket_t& client);
void run_async_test(zmq::socket_t& client);
void client_task();
void worker_task();
void broker_task();

chrono::time_point<chrono::system_clock> start_test(const string& test_name);
void end_test(const chrono::time_point<chrono::system_clock>& started, const size_t& call_count);

deque<string> recv_frames(zmq::socket_t& socket);
void send_frames(zmq::socket_t& socket, const deque<string>& frames);

static const char client_id[] = "Client";
static const char worker_id[] = "Worker";
static const char broker_id[] = "Broker";

mutex mut;

int main(int argc, char* argv[]) {
	thread client(client_task);
	thread worker(worker_task);
	thread broker(broker_task);

	client.join();
	worker.detach();
	broker.detach();
	
	cout << "Finished!" << endl;
	getchar();
	return 0;
}

void run_sync_test(zmq::socket_t& client) {
	auto start = start_test("Synchronous round-trip test");
	for (size_t i = 0; i < 10'000; ++i) {
		auto val(to_string(i));
		client.send(val.c_str(), val.size());

		zmq::message_t reply;
		client.recv(&reply);
	}

	end_test(start, 10'000);
}

void run_async_test(zmq::socket_t& client) {
	auto start = start_test("Asynchronous round-trip test");
	for (size_t i = 0; i < 100'000; ++i) {
		auto val(to_string(i));
		client.send(val.c_str(), val.size());
	}

	cout << "Receiving..." << endl;

	for (size_t i = 0; i < 100'000; ++i) {
		zmq::message_t reply;
		client.recv(&reply);

		auto val = stoi(string(static_cast<char*>(reply.data()), reply.size()));

		if (i % 1'000 == 0)
			cout << i << ": " << val << endl;
	}
	cout << endl;

	end_test(start, 100'000);
}

void client_task() {
	zmq::context_t context(1);
	zmq::socket_t client(context, ZMQ_DEALER);
	client.setsockopt(ZMQ_IDENTITY, client_id, strlen(client_id));
	client.connect("tcp://localhost:5555");

	cout << "Setting up test..." << endl;

	/* Note that the client thread does a small pause before starting. 
	This is to get around one of the "features" of the router socket: 
	if you send a message with the address of a peer that's not yet connected, 
	the message gets discarded. 
	In this example we don't use the load balancing mechanism, 
	so without the sleep, if the worker thread is too slow to connect, 
	it will lose messages, making a mess of our test. */
	this_thread::sleep_for(200ms);

	//run_sync_test(client);
	run_async_test(client);
}

void worker_task() {
	zmq::context_t context(1);
	zmq::socket_t worker(context, ZMQ_DEALER);
	worker.setsockopt(ZMQ_IDENTITY, worker_id, strlen(worker_id));
	worker.connect("tcp://localhost:5556");

	auto counter = 0L;

	while (true) {
		zmq::message_t request;
		worker.recv(&request);

		string val(static_cast<char*>(request.data()), request.size());

		auto num = stoi(val);
		if (num == 99'999)
			cout << "Final send.";

		worker.send(val.c_str(), val.size());

		if (num == 99'999)
			cout << " Sent.";
	}
}

void broker_task() {
	zmq::context_t context(1);
	zmq::socket_t frontend(context, ZMQ_ROUTER);
	zmq::socket_t backend(context, ZMQ_ROUTER);
	frontend.bind("tcp://*:5555");
	backend.bind("tcp://*:5556");

	zmq::pollitem_t items[] = {
		{ frontend, 0, ZMQ_POLLIN, 0 },
		{ backend, 0, ZMQ_POLLIN, 0 }
	};
	while (true) {
		zmq::poll(items, 2, -1);
		if (items[0].revents & ZMQ_POLLIN) {
			auto frames = recv_frames(frontend);
			frames.pop_front();
			frames.push_front(worker_id);

			send_frames(backend, frames);
		}
		if (items[1].revents & ZMQ_POLLIN) {
			auto frames = recv_frames(backend);
			frames.pop_front();
			frames.push_front(client_id);

			send_frames(frontend, frames);
		}
	}
}

chrono::time_point<chrono::system_clock> start_test(const string& test_name) {
	cout << test_name << " started..." << endl;
	return chrono::system_clock::now();
}

void end_test(const chrono::time_point<chrono::system_clock>& started, const size_t& call_count) {
	auto elapsed = chrono::system_clock::now() - started;
	auto ms = chrono::duration_cast<chrono::milliseconds>(elapsed);
	auto calls_per_sec = (call_count * 1000) / ms.count();

	cout << calls_per_sec << " calls/sec." << endl;
}

deque<string> recv_frames(zmq::socket_t& socket) {
	deque<string> frames;

	auto more_frames = TRUE;
	auto more_size = sizeof(more_frames);

	while (more_frames) {
		zmq::message_t message;
		socket.recv(&message);
		frames.push_back(string(static_cast<char*>(message.data()), message.size()));

		socket.getsockopt(ZMQ_RCVMORE, &more_frames, &more_size);
	}

	return frames;
}

void send_frames(zmq::socket_t& socket, const deque<string>& frames) {
	for (size_t i = 0; i < frames.size() - 1; i++)
		socket.send(frames[i].c_str(), frames[i].size(), ZMQ_SNDMORE);
	socket.send(frames.back().c_str(), frames.back().size());
}

