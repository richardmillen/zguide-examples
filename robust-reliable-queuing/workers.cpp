#include "workers.hpp"
#include "ppcommon.hpp"

#include <string>
#include <unordered_map>
#include <chrono>
#include <algorithm>
using namespace std;

#include <assert.h>

pp::workers_t::workers_t(std::chrono::seconds ttl) 
	: ttl_(ttl) {
}

bool pp::workers_t::empty() const {
	return queue_.empty();
}

void pp::workers_t::erase(const string & identity) {
	print_debug("workers_t", "Erasing '" + identity + "'...");

	lookup_.erase(identity);

	auto it = find_if(queue_.begin(), queue_.end(), [&](string& match) {
		return match == identity;
	});

	if (it != queue_.end())
		queue_.erase(it);

	print_debug("workers_t", "Erased.");
}

// push adds the identity of a worker to the queue. the identity must not already exist.
void pp::workers_t::push(const string & identity) {
	print_debug("workers_t", "Pushing identity '" + identity + "'...");

	assert(lookup_.count(identity) == 0);
	lookup_.insert(make_pair(identity, chrono::system_clock::now() + ttl_));
	queue_.push_back(identity);

	print_debug("workers_t", "Pushed.");
}

// try_push adds the identity of a worker to the queue if it's not already present.
bool pp::workers_t::try_push(const std::string & identity) {
	print_debug("workers_t", "Trying to push worker onto queue...");

	if (lookup_.count(identity))
		return false;
	push(identity);
	return true;
}

void pp::workers_t::refresh(const std::string & identity) {
	print_debug("workers_t", "Refreshing " + identity + "...");

	assert(lookup_.count(identity));
	lookup_[identity] = chrono::system_clock::now() + ttl_;

	print_debug(identity, "Refreshed.");
}

const std::string& pp::workers_t::front() const {
	print_debug("workers_t", "front: '" + queue_.front() + "'.");

	return queue_.front();
}

void pp::workers_t::pop() {
	if (queue_.empty())
		return;

	lookup_.erase(queue_.front());
	queue_.pop_front();
}

// purge efficiently removes dead workers from the queue (and the expiry lookup).
void pp::workers_t::purge() {
	print_debug("workers_t", "Purging (" + to_string(queue_.size()) + ") workers...");

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

	print_debug("workers_t", "Purged (" + to_string(queue_.size()) + " workers remaining.)");
}

