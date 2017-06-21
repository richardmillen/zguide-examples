#pragma once

#include <zmq.hpp>

#include <string>
#include <deque>

class flcliapi_t {
public:
	flcliapi_t();
	void connect(const std::string& endpoint);
	std::deque<std::string> request(const std::string& content);
private:

private:
	zmq::context_t context_;
};

flcliapi_t::flcliapi_t() 
	: context_{ 1 }
{}

void flcliapi_t::connect(const std::string& endpoint) {

}

std::deque<std::string> flcliapi_t::request(const std::string& content) {
	return std::deque<std::string>();
}
