#pragma

#include <string>
#include <chrono>
#include <functional>

namespace fl3 {

	class flcliserver_t {
	public:
		flcliserver_t(const std::string& endpoint);
	public:
		const std::string& endpoint() const;
		void alive(const bool& v);
		bool flcliserver_t::alive() const;
		std::chrono::time_point<std::chrono::system_clock> ping_at() const;
		bool expired() const;
		void next_ping(const std::chrono::milliseconds& interval);
	public:
		struct hash {
			size_t operator()(const flcliserver_t& server) const;
		private:
			std::hash<std::string> impl_;
		};
	private:
		std::string endpoint_;
		bool alive_;
		std::chrono::time_point<std::chrono::system_clock> ping_at_;
		std::chrono::time_point<std::chrono::system_clock> expires_;
	};

	flcliserver_t::flcliserver_t(const std::string& endpoint) 
		: endpoint_{ endpoint } 
	{}

	const std::string& flcliserver_t::endpoint() const {
		return endpoint_;
	}

	void flcliserver_t::alive(const bool& v) {
		alive_ = v;
	}

	bool flcliserver_t::alive() const {
		return alive_;
	}

	std::chrono::time_point<std::chrono::system_clock> flcliserver_t::ping_at() const {
		return ping_at_;
	}

	bool flcliserver_t::expired() const {
		return std::chrono::system_clock::now() >= expires_;
	}

	void flcliserver_t::next_ping(const std::chrono::milliseconds& interval) {
		ping_at_ = std::chrono::system_clock::now() + interval;
	}

	// equality operator overloaded for unordered_set.
	inline bool operator==(const flcliserver_t& lhs, const flcliserver_t& rhs) {
		return lhs.endpoint() == rhs.endpoint();
	}

	// function call operator overloaded for unordered_set.
	size_t flcliserver_t::hash::operator()(const flcliserver_t& server) const {
		return impl_(server.endpoint_);
	}
}