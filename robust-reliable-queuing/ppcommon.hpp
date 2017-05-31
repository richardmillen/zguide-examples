#pragma once

#include <zmq.hpp>

#include <iostream>
#include <string>
#include <deque>
using namespace std;

namespace pp
{
	deque<string> recv_frames(zmq::socket_t& socket) {
		deque<string> frames;
		auto more_frames = TRUE;

		while (more_frames) {
			zmq::message_t frame;
			socket.recv(&frame);
			frames.push_back(string(static_cast<char*>(frame.data()), frame.size()));

			more_frames = 0;
			auto more_size = sizeof(more_frames);
			socket.getsockopt(ZMQ_RCVMORE, &more_frames, &more_size);
		}

		return frames;
	}

	void send_frames(zmq::socket_t& socket, deque<string>& frames) {
		auto last = frames.end() - 1;
		for (auto it = frames.begin(); it < last; ++it) {
			socket.send(it->c_str(), it->size(), ZMQ_SNDMORE);
		}
		socket.send(frames.back().c_str(), frames.back().size());
	}

	void print_frames(deque<string>& frames) {
		for (auto& f : frames)
			cout << "Frame: " << f << endl;
	}
}