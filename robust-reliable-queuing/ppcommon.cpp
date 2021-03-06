#include "ppcommon.hpp"

#include <zmq.hpp>

#include <iostream>
#include <tuple>
#include <iostream>
#include <string>
#include <deque>
using namespace std;

bool pp::check_version(int wants_major, int wants_minor) {
	auto ver = zmq::version();

	if (get<0>(ver) > wants_major)
		return true;
	if (get<0>(ver) == wants_major && get<1>(ver) >= wants_minor)
		return true;

	cerr << "You're using 0MQ " << get<0>(ver) << "." << get<1>(ver) << ".\n" <<
		"You need at least " << wants_major << "." << wants_minor << "." << endl;

	return false;
}

deque<string> pp::recv_frames(zmq::socket_t& socket) {
	deque<string> frames;
	auto more_frames = TRUE;

	while (more_frames) {
		zmq::message_t frame;
		if (!socket.recv(&frame))
			return deque<string>();

		frames.push_back(string(static_cast<char*>(frame.data()), frame.size()));

		more_frames = 0;
		auto more_size = sizeof(more_frames);
		socket.getsockopt(ZMQ_RCVMORE, &more_frames, &more_size);
	}

	return frames;
}

void pp::send_frames(zmq::socket_t& socket, deque<string>& frames) {
	auto last = frames.end() - 1;
	for (auto it = frames.begin(); it < last; ++it) {
		socket.send(it->c_str(), it->size(), ZMQ_SNDMORE);
	}
	socket.send(frames.back().c_str(), frames.back().size());
}

void pp::print_frames(deque<string>& frames) {
	for (auto& f : frames)
		cout << "Frame: " << f << endl;
}

bool pp::was_interrupted(const deque<string> received_frames) {
	return received_frames.empty();
}

/* TODO: try conio.h [textcolor() + textbackground()] if this doesn't work.
https://stackoverflow.com/questions/9965710/how-to-change-text-and-background-color */

void pp::print_debug(const string & id, const string & msg) {
	cerr << "D: " << id << ": " << msg << endl;
}

void pp::print_info(const string & id, const string & msg) {
	cout << "I: " << id << ": " << msg << endl;
}

void pp::print_warn(const string & id, const string & msg) {
	cerr << "W: " << id << ": " << msg << endl;
}

void pp::print_error(const string & id, const string & msg) {
	cerr << "E: " << id << ": " << msg << endl;
}
