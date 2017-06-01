#include "workers.hpp"

#include <string>
#include <unordered_map>
#include <chrono>
#include <algorithm>
using namespace std;

pp::workers_t::workers_t(std::chrono::seconds ttl) 
	: ttl_(ttl) {
}

bool pp::workers_t::empty() const {
	return queue_.empty();
}

void pp::workers_t::erase(const string & identity) {
	lookup_.erase(identity);

	auto it = find_if(queue_.begin(), queue_.end(), [&](string& match) {
		return match == identity;
	});
	queue_.erase(it);
}

void pp::workers_t::push(const string & identity) {
	lookup_.insert(make_pair(identity, chrono::system_clock::now() + ttl_));
	queue_.push_back(identity);
}

void pp::workers_t::refresh(const std::string & identity) {
	lookup_[identity] = chrono::system_clock::now() + ttl_;
}

const std::string& pp::workers_t::front() const {
	return queue_.front();
}

void pp::workers_t::pop() {
	queue_.pop_front();
}

// purge efficiently removes dead workers from the queue (and the expiry lookup).
void pp::workers_t::purge() {
	for (size_t i = 0; i < queue_.size(); ++i) {
		if (chrono::system_clock::now() > lookup_[queue_.front()]) {
			lookup_.erase(queue_.front());
			queue_.pop_front();
		}
		else {
			queue_.push_back(queue_.front());
			queue_.pop_front();
		}
	}
}

