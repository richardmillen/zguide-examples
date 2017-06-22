#pragma once

/* we build the agent as a class that's capable of
 * processing messages coming in from its various sockets. */

#include "flmsg.hpp"

#include <thread>
#include <memory>

#include <zmq.hpp>

namespace fl3 {

	class flcliagent_t {
	public:
		flcliagent_t(zmq::context_t& context);
		~flcliagent_t();

		flmsg_t recv();
		void send(std::initializer_list<flframe_t> frames);
	private:
		void agent_task();
	private:
		zmq::context_t& context_;					// shared context (differs from example; 'own context')
		zmq::socket_t router_;						// socket to talk to servers
		std::unique_ptr<zmq::socket_t> apipipe_;	// socket application/api uses to talk to us
		std::unique_ptr<zmq::socket_t> pipe_;		// socket to talk back to application/api
		std::unique_ptr<std::thread> thread_;
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
	 * sockets and processes incoming messages */
	void flcliagent_t::agent_task() {

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