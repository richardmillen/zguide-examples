#include <zmq.hpp>

#include <boost/asio/ip/host_name.hpp>
namespace ip = boost::asio::ip;

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <deque>
#include <thread>
#include <random>
#include <chrono>
#include <algorithm>
using namespace std;

const char* WORKER_READY = "\001";

void worker_proc(string conn_addr, size_t num);
void client_proc(string conn_addr, size_t num);
deque<string> recv_frames(zmq::socket_t& socket);
bool send_frames(zmq::socket_t& socket, const deque<string>& frames, const size_t from_index = 0);
inline bool was_interrupted(int rc);
inline bool was_interrupted(bool res);
inline bool was_interrupted(const deque<string>& frames);

int main(int argc, char* argv[]) {
	try {
		ostringstream oss;
		int cloudfe_port;
		vector<string> peers;
		int localfe_port;
		int localbe_port;
		int client_count;
		int worker_count;
		po::options_description desc("Options");
		desc.add_options()
			("help", "prints this help message")
			("cloud-port", po::value<int>(&cloudfe_port)->required(), "port number for cloud peers (other brokers) to connect to us on")
			("peer", po::value<vector<string>>(&peers)->required(), "<address:port> of another cloud broker to connect to")
			("frontend-port", po::value<int>(&localfe_port)->required(), "local frontend port number to receive requests on")
			("backend-port", po::value<int>(&localbe_port)->required(), "local backend port number for workers to connec to us on")
			("clients", po::value<int>(&client_count)->default_value(10), "number of clients to start")
			("workers", po::value<int>(&worker_count)->default_value(3), "number of workers to start")
			;

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
		
		if (vm.count("help")) {
			cout << "Usage: peering2 [options]\n" << endl;
			cout << desc << endl;
			return 0;
		}

		po::notify(vm);

		auto fe_id("localhost:" + to_string(cloudfe_port));
		auto be_id(fe_id);

		zmq::context_t context(1);

		zmq::socket_t cloudfe(context, ZMQ_ROUTER);
		cloudfe.setsockopt(ZMQ_IDENTITY, fe_id.c_str(), fe_id.size());
		cloudfe.bind("tcp://*:" + to_string(cloudfe_port));

		zmq::socket_t cloudbe(context, ZMQ_ROUTER);
		cloudbe.setsockopt(ZMQ_IDENTITY, be_id.c_str(), be_id.size());
		
		for (const auto& peer : peers) {
			cloudbe.connect("tcp://" + peer);

			cout << "peering2: Connected to " << peer << "." << endl;
		}

		zmq::socket_t localfe(context, ZMQ_ROUTER);
		localfe.bind("tcp://*:" + to_string(localfe_port));

		zmq::socket_t localbe(context, ZMQ_ROUTER);
		localbe.bind("tcp://*:" + to_string(localbe_port));

		cout << "Press enter when all brokers are started: ";
		getchar();

		vector<thread> workers;
		for (size_t worker_nbr = 0; worker_nbr < worker_count; ++worker_nbr) {
			workers.push_back(thread(worker_proc, worker_nbr));
		}

		vector<thread> clients;
		for (size_t client_nbr = 0; client_nbr < client_count; ++client_nbr) {
			clients.push_back(thread(client_proc, client_nbr));
		}

		random_device rd;
		mt19937 eng(rd());
		uniform_int_distribution<> route_distr(0, 5);
		uniform_int_distribution<> broker_distr(0, peers.size() - 1);

		/*
		INTERESTING PART:
		Here, we handle the request-reply flow. We're using load-balancing
		to poll workers at all times, and clients only when there are one or more
		workers available.
		*/

		deque<string> ready_workers;
		auto next_frame_idx = 1;

		while (true) {
			zmq::pollitem_t backends[] = {
				{ localbe, 0, ZMQ_POLLIN, 0 },
				{ cloudbe, 0, ZMQ_POLLIN, 0 }
			};

			if (was_interrupted(zmq::poll(backends, 2, ready_workers.empty() ? -1 : 1000)))
				break;

			deque<string> frames;
			if (backends[0].revents & ZMQ_POLLIN) {
				frames = recv_frames(localbe);

				if (was_interrupted(frames))
					break;

				ready_workers.push_back(frames[0]);

				// TODO: get rid of this if statement - figure out if we have an empty / delimiter frame and write the following section accordingly.
				if (frames[next_frame_idx].size() == 0)
					++next_frame_idx;

				if (frames[next_frame_idx].compare(WORKER_READY) == 0) {
					continue;
				}
			}
			else if (backends[1].revents & ZMQ_POLLIN) {
				frames = recv_frames(cloudbe);

				if (was_interrupted(frames))
					break;

				// TODO: get rid of this if statement (as above)
				if (frames[next_frame_idx].size() == 0)
					++next_frame_idx;
			}

			for (auto& broker : peers) {
				if (frames[next_frame_idx].compare(broker) == 0) {
					send_frames(cloudfe, frames, next_frame_idx);
					frames.clear();
				}
			}

			if (!frames.empty())
				send_frames(localfe, frames, next_frame_idx);

			// route as many client requests as we have workers available
			// we may reroute requests from our local frontend, but not from
			// the cloud frontend. we reroute randomly now, just to test things
			// out. in the next version, we'll do this properly by calculating
			// cloud capacity.

			while (ready_workers.size()) {
				zmq::pollitem_t frontends[] = {
					{ localfe, 0, ZMQ_POLLIN, 0 },
					{ cloudfe, 0, ZMQ_POLLIN, 0 }
				};

				auto rc = zmq::poll(frontends, 2, 0);
				assert(rc >= 0);

				auto reroutable = false;

				if (frontends[1].revents & ZMQ_POLLIN) {
					frames = recv_frames(cloudfe);
				}
				else if (frontends[0].revents & ZMQ_POLLIN) {
					frames = recv_frames(localfe);
					reroutable = true;
				}
				else {
					// no work, go back to backends
					break;
				}

				// if reroutable, send to cloud 20% of the time
				// here we'd normally use cloud status information

				if (reroutable && peers.size() > 0 && route_distr(eng) == 0) {
					// route to random broker
					auto& broker = peers.at(broker_distr(eng));
					frames.push_front(broker);
					send_frames(cloudbe, frames);
				}
				else {
					auto id(ready_workers[0]);
					ready_workers.pop_front();

					frames.push_front(string());
					frames.push_front(id);

					send_frames(localbe, frames);
				}
			}
		}
	}
	catch (exception& e) {
		cerr << e.what() << endl;
		return 1;
	}
	return 0;
}

// worker_proc is the worker task, which plugs 
// into the load-balancer using a REQ socket.
void worker_proc(string conn_addr, size_t num) {
	zmq::context_t context(1);
	zmq::socket_t worker(context, ZMQ_REQ);
	worker.connect("tcp://" + conn_addr);

	cout << "Worker #" << num << ": Connected to " << conn_addr << endl;

	worker.send(WORKER_READY, strlen(WORKER_READY));

	while (true) {
		auto frames = recv_frames(worker);

		if (was_interrupted(frames))
			break;

		cout << "Worker #" << num << ": " << frames.back() << endl;
		frames.back().assign("OK");

		send_frames(worker, frames);
	}
}

// client_proc is the client task, which does a request-reply
// dialog using a standard synchronous REQ socket.
void client_proc(string conn_addr, size_t num) {
	zmq::context_t context(1);
	zmq::socket_t client(context, ZMQ_REQ);
	client.connect("tcp://" + conn_addr);

	cout << "Client #" << num << ": Connected to " << conn_addr << endl;
	
	while (true) {
		client.send("HELLO", 5);

		zmq::message_t reply;
		if (was_interrupted(client.recv(&reply)))
			break;

		string str(static_cast<char*>(reply.data()), reply.size());

		cout << "Client #" << num << ": " << str << endl;
	}
}

deque<string> recv_frames(zmq::socket_t& socket) {
	deque<string> frames;

	auto more_frames = TRUE;
	while (more_frames) {
		zmq::message_t message;
		if (was_interrupted(socket.recv(&message)))
			return deque<string>();

		frames.push_back(string(static_cast<char*>(message.data()), message.size()));

		more_frames = 0;
		auto more_size = sizeof(more_frames);

		socket.getsockopt(ZMQ_RCVMORE, &more_frames, &more_size);
	}

	return frames;
}

// TODO: this isn't making use of the bool return value. is it needed?
bool send_frames(zmq::socket_t& socket, const deque<string>& frames, const size_t from_index) {
	auto from = frames.begin() + from_index;
	auto to = frames.end() - 1;

	for_each(from, to, [&socket](const string& f) {
		socket.send(f.c_str(), f.size(), ZMQ_SNDMORE);
	});

	auto last = frames.back();
	socket.send(last.c_str(), last.size());

	return true;
}

/*
TODO: was_interrupted: check for error state to ensure we do in fact have an ETERM and nothing serious!
*/

inline bool was_interrupted(const deque<string>& frames) {
	return frames.size() == 0;
}

inline bool was_interrupted(int rc) {
	if (rc == -1)
		return true;
	return false;
}

inline bool was_interrupted(bool res) {
	return !res;
}
