#include <zmq.hpp>
#include <iostream>

int main(int argc, char* argv[]) {
	int major, minor, patch;
	zmq_version(&major, &minor, &patch);
	std::cout << "Current 0MQ version is " << major << "." << minor << "." << patch << std::endl;
	std::getchar();
	return 0;
}
