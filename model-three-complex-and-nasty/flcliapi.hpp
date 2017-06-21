#pragma once

#include <zmq.hpp>

#include <string>
#include <deque>
#include <thread>

using flmsg = std::deque<std::string>;
using flframe = std::string;

class flcliapi_t {
public:
	flcliapi_t();
	~flcliapi_t();
	void connect(const std::string& endpoint);
	flmsg request(const flframe& content);
private:
	flmsg recv();
	void send(flmsg msg);
	void agent_proc();
private:
	zmq::context_t context_;
	zmq::socket_t pipe_;
	std::thread agent_;
};

flcliapi_t::flcliapi_t() 
	: context_{ 1 }, agent_{ &agent_proc }, pipe_{ context_, ZMQ_ROUTER }
{}

flcliapi_t::~flcliapi_t() {
	agent_.join();
}

/* to implement the connect method, the frontend object sends a multipart
 * message to the backend agent. the first part is a string "CONNECT", and
 * the second part is the endpoint. it waits 100ms for the connection to
 * come up, which isn't pretty, but saves us from sending all requests to a
 * single server, at startup time. */
void flcliapi_t::connect(const std::string& endpoint) {
	pipe_.send("CONNECT", 7, ZMQ_SNDMORE);
	pipe_.send(endpoint.c_str(), endpoint.size());

	std::this_thread::sleep_for(100ms);
}

/* to implement the request method, the frontend object sends a message
 * to the backend, specifying a command "REQUEST" and the request message */
flmsg flcliapi_t::request(const flframe& content) {
	pipe_.send("REQUEST", 7, ZMQ_SNDMORE);
	pipe_.send(content.c_str(), content.size());

	zmq::message_t status_msg;
	if (!pipe_.recv(&status_msg))
		return flmsg();

	auto status(string(static_cast<char*>(status_msg.data()), status_msg.size()));
	if (status == "FAILED")
		return flmsg();

	flmsg reply;
	reply.push_back(status);

	// https://github.com/richardmillen/zguide-examples/issues/23
	// http://zguide.zeromq.org/page:all#Model-Three-Complex-and-Nasty

	return reply;
}

flmsg flcliapi_t::recv() {
	auto recv_and_more = [&]() {
		zmq::message_t m;
		pipe_.recv(&m);
		msg.push_back(string(static_cast<char*>(m.data()), m.size()));
		return pipe_.getsockopt<int>(ZMQ_RCVMORE);
	};
	
	/* TODO: figure out how to write it something like: 
		return flmsg(std::begin(recv_lambda), std::end(recv_lambda)) */
	return flmsg(.......);
}

void flcliapi_t::send(flmsg msg) {
	auto last = msg.end() - 1;
	for (auto& it = msg.begin(); it != last; ++it)
		pipe_.send(it->c_str(), it->size(), ZMQ_SNDMORE);
	pipe_.send(last->c_str(), last->size());
}

void flcliapi_t::agent_proc() {

}

// Frame 0: identity of server
// Frame 1: PONG, or control frame
// Frame 2: "OK"
