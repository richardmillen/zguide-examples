#pragma once

/* we build the agent as a class that's capable of
 * processing messages coming in from its various sockets. */

#include "flmsg.hpp"

#include <thread>
#include <memory>
#include <chrono>

#include <zmq.hpp>

namespace fl3 {

	class flcliagent_t {
	public:
		flcliagent_t(zmq::context_t& context);
		~flcliagent_t();
	private:
		void agent_task();
		void on_control_message();
		void on_router_message();
	public:
		flmsg_t recv();
		void send(std::initializer_list<flframe_t> frames);
	private:
		zmq::context_t& context_;					// shared context (differs from example; 'own context')
		zmq::socket_t router_;						// socket to talk to servers
		std::unique_ptr<zmq::socket_t> apipipe_;	// socket application/api uses to talk to us
		std::unique_ptr<zmq::socket_t> pipe_;		// socket to talk back to application/api
		std::unique_ptr<std::thread> thread_;
	private:
		std::chrono::time_point<std::chrono::system_clock> expires_;
		flmsg_t request_;
	};

	flcliagent_t::flcliagent_t(zmq::context_t& context)
		: context_{ context }, router_{ context_, ZMQ_ROUTER } {

		apipipe_ = std::make_unique<zmq::socket_t>(context_, ZMQ_PAIR);
		apipipe_->bind("inproc://agent");

		pipe_ = std::make_unique<zmq::socket_t>(context_, ZMQ_PAIR);
		pipe_->connect("inproc://agent");

		thread_ = std::make_unique<std::thread>(&agent_task);
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

			// TODO: zhash_foreach(servers, server_tickless, &tickless);

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

				// https://github.com/richardmillen/zguide-examples/issues/23
				// http://zguide.zeromq.org/page:all#Model-Three-Complex-and-Nasty
			}
			// disconnect and delete any expired servers
			// send heartbeats to idle servers if needed
			// TODO: zhash_foreach(servers, server_ping, router);
		}
	}

	/* 
	 */
	void flcliagent_t::on_control_message() {

	}

	/* 
	 */
	void flcliagent_t::on_router_message() {

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

	// Frame 0: identity of server
	// Frame 1: PONG, or control frame
	// Frame 2: "OK"

}