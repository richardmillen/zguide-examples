#pragma once

#include <zmq.hpp>

#include <string>
#include <deque>
#include <initializer_list>

namespace fl3 {

	using flframe_t = std::string;
	using flmsg_t = std::deque<flframe_t>;

	namespace utils {

		flmsg_t recv_msg(zmq::socket_t& socket) {
			flmsg_t msg;
			auto recv_msg = [&]() {
				zmq::message_t m;
				socket.recv(&m);
				msg.push_back(std::string(static_cast<char*>(m.data()), m.size()));
				return socket.getsockopt<int>(ZMQ_RCVMORE);
			};
			while (recv_msg())
				;
			return msg;
		}

		void send_msg(zmq::socket_t& socket, flmsg_t msg) {
			auto last = msg.end() - 1;
			for (auto& it = msg.begin(); it != last; ++it)
				socket.send(it->c_str(), it->size(), ZMQ_SNDMORE);
			socket.send(last->c_str(), last->size());
		}

		void send_msg(zmq::socket_t& socket, flframe_t frame) {
			socket.send(frame.c_str(), frame.size());
		}
	}
}