#pragma once

#include <chrono>
#include <string>
#include <deque>
#include <unordered_map>

namespace pp
{
	// TODO: try updating to use boost::multi_index_container.
	// http://www.boost.org/doc/libs/1_39_0/libs/multi_index/doc/index.html
	class workers_t
	{
	public:
		workers_t(std::chrono::seconds ttl);

		bool empty() const;
		void erase(const std::string& identity);
		void push(const std::string& identity);
		bool try_push(const std::string& identity);
		void refresh(const std::string& identity);
		const std::string& front() const;
		void pop();
		void purge();
		
		auto pp::workers_t::begin() {
			return queue_.begin();
		}

		auto pp::workers_t::end() {
			return queue_.end();
		}

	private:
		std::chrono::seconds ttl_;
		std::deque<std::string> queue_;
		std::unordered_map<std::string, std::chrono::time_point<std::chrono::system_clock>> lookup_;
	};
}