#pragma once

#include "flcliagent.hpp"
#include "flmsg.hpp"

#include <zmq.hpp>

#include <string>
#include <deque>
#include <thread>
#include <chrono>
#include <initializer_list>

namespace fl3 {

	class flcliapi_t {
	public:
		flcliapi_t();
		void connect(const std::string& endpoint);
		flmsg_t request(const flframe_t& content);
	private:
		zmq::context_t context_;
		flcliagent_t agent_;
	};

	flcliapi_t::flcliapi_t()
		: context_{ 1 }, agent_{ context_ }
	{}

	/* to implement the connect method, the frontend object sends a multipart
	 * message to the backend agent. the first part is a string "CONNECT", and
	 * the second part is the endpoint. it waits 100ms for the connection to
	 * come up, which isn't pretty, but saves us from sending all requests to a
	 * single server, at startup time. */
	void flcliapi_t::connect(const std::string& endpoint) {
		agent_.send({ "CONNECT", endpoint });

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	/* to implement the request method, the frontend object sends a message
	 * to the backend, specifying a command "REQUEST" and the request message */
	flmsg_t flcliapi_t::request(const flframe_t& content) {
		agent_.send({ "REQUEST", content });

		auto reply = agent_.recv();
		if (reply.empty())
			return reply;

		auto status = reply.front();
		reply.pop_front();
		if (status == "FAILED")
			return flmsg_t();

		return reply;
	}
}