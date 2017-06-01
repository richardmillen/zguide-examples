#pragma once

#include <zmq.hpp>

#include <iostream>
#include <string>
#include <deque>
#include <chrono>

namespace pp // paranoid pirate
{
	static const unsigned heartbeat_liveness = 3; // 3-5 is reasonable
	static const std::chrono::seconds heartbeat_interval = std::chrono::seconds(1);
	static const std::chrono::seconds ttl = std::chrono::seconds(heartbeat_interval * heartbeat_liveness);

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

	void send_frames(zmq::socket_t& socket, std::deque<std::string>& frames) {
		auto last = frames.end() - 1;
		for (auto it = frames.begin(); it < last; ++it) {
			socket.send(it->c_str(), it->size(), ZMQ_SNDMORE);
		}
		socket.send(frames.back().c_str(), frames.back().size());
	}

	void print_frames(std::deque<std::string>& frames) {
		for (auto& f : frames)
			std::cout << "Frame: " << f << std::endl;
	}

	bool was_interrupted(const std::deque<std::string> received_frames) {
		return received_frames.empty();
	}
}
