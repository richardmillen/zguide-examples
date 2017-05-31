#include "workers.hpp"

struct worker_t
{

};

pp::workers_t::workers_t()
{
}

pp::workers_t::~workers_t()
{
}

workers_t & pp::workers_t::operator++()
{
	// TODO: insert return statement here
}

bool pp::workers_t::empty() const
{
	return false;
}

void pp::workers_t::erase(const std::string & identity)
{
}

void pp::workers_t::push(const std::string & identity)
{
}

void pp::workers_t::refresh(const std::string & identity)
{
}

std::string pp::workers_t::front() const
{
	return std::string();
}

void pp::workers_t::pop()
{
}
