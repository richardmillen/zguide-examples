#pragma once

/* we build the agent as a class that's capable of
 * processing messages coming in from its various sockets. */

#include "flmsg.hpp"
#include "flcliserver.hpp"

#include <iostream>
#include <thread>
#include <memory>
#include <chrono>
#include <deque>
#include <unordered_map>
#include <functional>

#include <zmq.hpp>

namespace fl3 {

#define GLOBAL_TIMEOUT			std::chrono::seconds(3)

	class flcliagent_t {
	public:
		flcliagent_t(zmq::context_t& context);
		~flcliagent_t();
	private:
		void agent_task();
		void on_control_message();
		void on_router_message();
		void ping(flcliserver_t& server);
		int stoseq(const std::string& str);
	public:
		flmsg_t recv();
		void send(std::initializer_list<flframe_t> frames);
	private:
		zmq::context_t& context_;														// shared context (differs from example; 'own context')
		zmq::socket_t router_;															// socket to talk to servers
		std::unique_ptr<zmq::socket_t> apipipe_;										// socket application/api uses to talk to us
		std::unique_ptr<zmq::socket_t> pipe_;											// socket to talk back to application/api
		std::unique_ptr<std::thread> thread_;											// agent thread
	private:
		std::chrono::time_point<std::chrono::system_clock> expires_;					// timeout for request/reply
		flmsg_t request_;																// current request if any
		flmsg_t reply_;																	// current reply if any
		std::deque<std::shared_ptr<flcliserver_t>> alive_servers_;						// servers we know are alive
		std::unordered_map<std::string, std::shared_ptr<flcliserver_t>> conn_map_;		// servers we're connected to (name-to-server table)
		unsigned sequence_;																// number of requests ever sent
	};

	flcliagent_t::flcliagent_t(zmq::context_t& context)
		: context_{ context }, router_{ context_, ZMQ_ROUTER }, 
		  request_{}, reply_{}, 
		  alive_servers_{}, conn_map_{},
		  sequence_{ 0 } {

		apipipe_ = std::make_unique<zmq::socket_t>(context_, ZMQ_PAIR);
		apipipe_->bind("inproc://agent");

		pipe_ = std::make_unique<zmq::socket_t>(context_, ZMQ_PAIR);
		pipe_->connect("inproc://agent");

		thread_ = std::make_unique<std::thread>(&flcliagent_t::agent_task, this);
	}

	flcliagent_t::~flcliagent_t() {
		thread_->join();
	}

	/* here's the agent task itself, which polls its two
	 * sockets and processes incoming messages. */
	void flcliagent_t::agent_task() {
		zmq::pollitem_t items[] = {
			{ *pipe_, 0, ZMQ_POLLIN, 0 },
			{ router_, 0, ZMQ_POLLIN, 0 }
		};

		while (true) {
			// calculate tickless timer, up to 1 hour
			auto tickless = std::chrono::system_clock::now() +
				std::chrono::hours(1);

			if (!request_.empty() && tickless > expires_)
				tickless = expires_;

			for (auto& pair : conn_map_) {
				auto server = pair.second;
				if (tickless > server->ping_at())
					tickless = server->ping_at();
			}

			auto timeout = tickless - std::chrono::system_clock::now();
			int rc = zmq::poll(items, 2, std::chrono::duration_cast<std::chrono::milliseconds>(timeout));
			if (rc == -1)
				break;					// context has been shut down

			if (items[0].revents & ZMQ_POLLIN)
				on_control_message();

			if (items[1].revents & ZMQ_POLLIN)
				on_router_message();

			// if we're processing a request, dispatch to next server
			if (!request_.empty()) {
				if (std::chrono::system_clock::now() >= expires_) {
					utils::send_msg(*pipe_, "FAILED");
					request_.clear();
				}
				else {
					// find server to talk to, remove any expired ones
					while (!alive_servers_.empty()) {
						auto server = alive_servers_.front();
						if (!server->expired()) {
							request_.push_front(server->endpoint());
							utils::send_msg(router_, request_);
							break;
						}
						alive_servers_.pop_front();
						server->alive(false);
					}
				}
			}

			for (auto& pair : conn_map_)
				ping(*(pair.second));
		}
	}

	/* this method processes one message from
	 * the pipe; the inproc pair socket connected
	 * to the client api */
	void flcliagent_t::on_control_message() {
		auto msg{ utils::recv_msg(*pipe_) };
		
		auto command{ msg.front() };
		msg.pop_front();

		if (command == "CONNECT") {
			auto endpoint{ msg.front() };

			std::cout << "flcliagent: connecting to " << endpoint << "..." << std::endl;
			router_.connect(endpoint);

			auto server{ std::make_shared<flcliserver_t>(endpoint) };
			conn_map_[endpoint] = server;
			alive_servers_.push_back(server);

			server->next_ping(PING_INTERVAL);
			server->ttl(SERVER_TTL);
		}
		else if (command == "REQUEST") {
			assert(request_.empty());			// strict request-reply cycle
			
			// prefix request with sequence number and empty envelope					<------ where's the empty envelope?
			msg.push_front(std::to_string(++sequence_));
			// take ownership of request message
			request_ = msg;
			// request expires after global timeout
			expires_ = std::chrono::system_clock::now() + GLOBAL_TIMEOUT;
		}
	}

	/* this method processes one message from
	 * a connected server.
	 * Frame 0: identity of server
	 * Frame 1: PONG, or control frame
	 * Frame 2: "OK" */
	void flcliagent_t::on_router_message() {
		auto reply = utils::recv_msg(router_);

		// frame 0 is server that replied
		auto endpoint{ reply.front() };
		reply.pop_front();

		assert(conn_map_.count(endpoint));
		auto& server = conn_map_[endpoint];
		
		if (!server->alive()) {
			alive_servers_.push_back(server);
			server->alive(true);
		}

		server->next_ping(PING_INTERVAL);
		server->ttl(SERVER_TTL);

		// frame 1 may be sequence number for reply
		auto sequence{ reply.front() };
		reply.pop_front();
		
		auto seq_no = stoseq(sequence);
		if (seq_no != -1 && seq_no != sequence_)
			return;

		reply.push_front("OK");
		utils::send_msg(*pipe_, reply);
		request_.clear();
	}

	/* disconnect and delete any expired servers
	 * send heartbeats to idle servers if needed */
	void flcliagent_t::ping(flcliserver_t& server) {
		if (std::chrono::system_clock::now() < server.ping_at())
			return;

		flmsg_t msg;
		msg.push_back(server.endpoint());
		msg.push_back("PING");
		
		utils::send_msg(router_, msg);
		server.next_ping(PING_INTERVAL);
	}

	/* converts the specified string into a valid unsigned 
	 * sequence number, or returns -1. 
	 * strtol sets 'end' to the value of 'str' if no conversion 
	 * was performed. */
	int flcliagent_t::stoseq(const std::string& str) {
		char* end;
		auto seq_no = strtol(str.c_str(), &end, 10);
		if (*end == 0)
			return seq_no;
		return -1;
	}

	/* recv wraps the api-to-agent call to receive message
	 * frames from a server (via the agent thread pipe). */
	flmsg_t flcliagent_t::recv() {
		return utils::recv_msg(*apipipe_);
	}

	/* send wraps the api-to-agent call to send message
	 * frames to a server (via the agent thread pipe). */
	void flcliagent_t::send(std::initializer_list<flframe_t> frames) {
		utils::send_msg(*apipipe_, flmsg_t{ frames });
	}
}