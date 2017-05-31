
#pragma once

#include <string>

namespace pp
{
	struct worker_t;

	// how do we support an ordered queue, with random removal? 
	// + boost::multi_index?
	// 
	// what are the conditions under which a worker will be removed from the queue?
	// + if the worker thinks the broker has gone (but hasn't) then comes back online.
	// + if the worker itself disappears the broker should remove it.
	// 
	// what about refreshing the 'last heartbeat' of the worker? that will also need random access.
	class workers_t
	{
	public:
		workers_t();
		~workers_t();

		workers_t& operator++();
		
		// HACK: the following operators were omitted from this example:
		// + prefix decrement
		// + postfix inc/dec operators

		bool empty() const;
		void erase(const std::string& identity);
		void push(const std::string& identity);
		void refresh(const std::string& identity);
		std::string front() const;
		void pop();

	private:
		
	};
}