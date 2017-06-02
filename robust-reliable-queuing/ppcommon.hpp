#pragma once

#include <zmq.hpp>

#include <iosfwd>
#include <string>
#include <deque>
#include <tuple>
#include <chrono>

namespace pp // paranoid pirate
{
	static const unsigned heartbeat_liveness = 3; // 3-5 is reasonable
	static const std::chrono::seconds heartbeat_interval = std::chrono::seconds(1);
	static const std::chrono::seconds ttl = std::chrono::seconds(heartbeat_interval * heartbeat_liveness);
	static const std::chrono::seconds interval_max = std::chrono::seconds(32);

	bool check_version(int wants_major, int wants_minor);
	std::deque<std::string> recv_frames(zmq::socket_t& socket);
	void send_frames(zmq::socket_t& socket, std::deque<std::string>& frames);
	void print_frames(std::deque<std::string>& frames);
	bool was_interrupted(const std::deque<std::string> received_frames);

	void print_debug(const std::string& id, const std::string& msg);
	void print_info(const std::string& id, const std::string& msg);
	void print_warn(const std::string& id, const std::string& msg);
	void print_error(const std::string& id, const std::string& msg);
}
