
#pragma once

#include <zmq.hpp>
#include <string>
#include <deque>

/*
******************************************************************************************
spcommon contains some utility functions used by both the simple pirate broker and worker.
******************************************************************************************
*/

namespace sp
{
	std::deque<std::string> recv_frames(zmq::socket_t& socket) {
		std::deque<std::string> frames;
		auto more_frames = TRUE;

		while (more_frames) {
			zmq::message_t frame;
			if (!socket.recv(&frame))
				return std::deque<std::string>();

			frames.push_back(std::string(static_cast<char*>(frame.data()), frame.size()));

			more_frames = 0;
			auto more_size = sizeof(more_frames);
			socket.getsockopt(ZMQ_RCVMORE, &more_frames, &more_size);
		}

		return frames;
	}

	void send_frames(zmq::socket_t& socket, const std::deque<std::string>& frames) {
		auto last = frames.end() - 1;
		for (auto it = frames.begin(); it != last; ++it)
			socket.send(it->c_str(), it->size(), ZMQ_SNDMORE);

		socket.send(frames.back().c_str(), frames.back().size());
	}
}