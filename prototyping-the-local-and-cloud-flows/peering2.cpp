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
#include <functional>
#include <mutex>
using namespace std;

const char* WORKER_READY = "\001";

mutex mut;

void worker_proc(const string conn_addr, const size_t num);
void client_proc(const string conn_addr, const size_t num);
deque<string> recv_frames(zmq::socket_t& socket);
bool send_frames(zmq::socket_t& socket, const deque<string>& frames);
void print_frames(const deque<string>& frames, const string& hint);
inline void sync(function<void()> fn);
inline bool was_interrupted(int rc);
inline bool was_interrupted(bool res);
inline bool was_interrupted(const deque<string>& frames);

/*
the broker part of this app uses four sockets:
	1) local frontend: clients connect to this. they send REQ(uest)s, and expect REP(ly)s.
	2) local backend: workers connect to this. the send REQ(uests)s for work, and expect REP(ly)s from us, containing the REQ(uest) sent by a client or peer broker.
	3) cloud frontend: peer brokers connect to this, in order to send REQ(uest)s to us on behalf of their own clients.
	4) cloud backend: we connect to this, in order to forward REQ(uest)s from our clients to peer brokers.

e.g.
./peering2 --cloud-port=5551 --peer=localhost:5552 --peer=localhost:5553 --frontend-port=5561 --backend-port=5562
./peering2 --cloud-port=5552 --peer=localhost:5551 --peer=localhost:5553 --frontend-port=5571 --backend-port=5572
./peering2 --cloud-port=5553 --peer=localhost:5551 --peer=localhost:5552 --frontend-port=5581 --backend-port=5582
*/
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

			cout << "peering2: Connected to peer broker '" << peer << "'." << endl;
		}

		zmq::socket_t localfe(context, ZMQ_ROUTER);
		localfe.bind("tcp://*:" + to_string(localfe_port));

		zmq::socket_t localbe(context, ZMQ_ROUTER);
		localbe.bind("tcp://*:" + to_string(localbe_port));

		cout << "peering2: Listening for peer broker requests on port #" << cloudfe_port << "..." << endl;
		cout << "peering2: Listening for client requests on port #" << localfe_port << "..." << endl;
		cout << "peering2: Listening for workers on port #" << localbe_port << "..." << endl;

		cout << "peering2: Press enter when all brokers are started: " << flush;
		getchar();

		cout << "peering2: Starting " << worker_count << " workers and " << client_count << " clients..." << endl;

		vector<thread> workers;
		for (size_t worker_nbr = 0; worker_nbr < worker_count; ++worker_nbr) {
			workers.push_back(thread(worker_proc, string("localhost:" + to_string(localbe_port)), worker_nbr));
		}

		vector<thread> clients;
		for (size_t client_nbr = 0; client_nbr < client_count; ++client_nbr) {
			clients.push_back(thread(client_proc, string("localhost:" + to_string(localfe_port)), client_nbr));
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
		
		while (true) {
			zmq::pollitem_t backends[] = {
				{ localbe, 0, ZMQ_POLLIN, 0 },
				{ cloudbe, 0, ZMQ_POLLIN, 0 }
			};

			if (was_interrupted(zmq::poll(backends, 2, ready_workers.empty() ? -1 : 1000)))
				break;

			deque<string> rep_frames;
			if (backends[0].revents & ZMQ_POLLIN) {
				rep_frames = recv_frames(localbe);

				if (was_interrupted(rep_frames))
					break;

				assert(rep_frames.front().empty() == false);
				
				auto id(rep_frames.front());
				ready_workers.push_back(id);
				rep_frames.pop_front();

				assert(rep_frames.front().empty());
				
				rep_frames.pop_front();
				
				if (rep_frames.front().compare(WORKER_READY) == 0) {
					rep_frames.clear();
				}
			}
			else if (backends[1].revents & ZMQ_POLLIN) {
				sync([]() {
					cout << "peering2: Receiving reply from peer broker..." << endl;
				});

				rep_frames = recv_frames(cloudbe);

				if (was_interrupted(rep_frames))
					break;

				// we don't use peer broker identity for anything
				rep_frames.pop_front();
			}

			if (!rep_frames.empty()) {
				for (auto& broker : peers) {
					if (rep_frames.front().compare(broker) == 0) {
						send_frames(cloudfe, rep_frames);
						rep_frames.clear();
						break;
					}
				}
			}

			if (!rep_frames.empty())
				send_frames(localfe, rep_frames);

			// route as many client requests as we have workers available
			// we may reroute requests from our local frontend, but not from
			// the cloud frontend. we reroute randomly now, just to test things
			// out. in the next version, we'll do this properly by calculating
			// cloud capacity.

			while (!ready_workers.empty()) {
				zmq::pollitem_t frontends[] = {
					{ cloudfe, 0, ZMQ_POLLIN, 0 },
					{ localfe, 0, ZMQ_POLLIN, 0 }
				};

				auto rc = zmq::poll(frontends, 2, 0);
				assert(rc >= 0);

				deque<string> req_frames;
				auto reroutable = false;

				// we'll do peer brokers first, to prevent starvation
				if (frontends[0].revents & ZMQ_POLLIN) {
					sync([]() {
						cout << "peering2: Receiving request from peer broker..." << endl;
					});

					req_frames = recv_frames(cloudfe);
				}
				else if (frontends[1].revents & ZMQ_POLLIN) {
					req_frames = recv_frames(localfe);
					reroutable = true;
				}
				else {
					// no work, go back to backends
					break;
				}

				// if reroutable, send to cloud 20% of the time
				// here we'd normally use cloud status information

				if (reroutable && !peers.empty() > 0 && route_distr(eng) == 0) {
					auto& broker = peers.at(broker_distr(eng));
					
					req_frames.push_front(broker);
					send_frames(cloudbe, req_frames);
				}
				else {
					auto id(ready_workers.front());
					ready_workers.pop_front();

					req_frames.push_front(string());
					req_frames.push_front(id);

					send_frames(localbe, req_frames);
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
void worker_proc(const string conn_addr, const size_t num) {
	zmq::context_t context(1);
	zmq::socket_t worker(context, ZMQ_REQ);
	worker.connect("tcp://" + conn_addr);

	sync([&]() {
		cout << "peering2: Worker #" << num << ": Sending ready message to " << conn_addr << endl;
	});

	zmq::message_t ready_msg(1);
	memcpy(ready_msg.data(), WORKER_READY, 1);
	worker.send(ready_msg);

	sync([&]() {
		cout << "peering2: Worker #" << num << ": Waiting for requests..." << endl;
	});

	while (true) {
		auto frames = recv_frames(worker);

		if (was_interrupted(frames))
			break;

		print_frames(frames, "Worker #" + to_string(num) + " received:");
		
		frames.back().assign("OK");

		send_frames(worker, frames);
	}
}

// client_proc is the client task, which does a request-reply
// dialog using a standard synchronous REQ socket.
void client_proc(const string conn_addr, const size_t num) {
	zmq::context_t context(1);
	zmq::socket_t client(context, ZMQ_REQ);
	client.connect("tcp://" + conn_addr);

	sync([&]() {
		cout << "peering2: Client #" << num << ": Connected to " << conn_addr << endl;
	});
	
	while (true) {
		zmq::message_t request(5);
		memcpy(request.data(), "HELLO", 5);
		client.send(request);

		zmq::message_t reply;
		if (was_interrupted(client.recv(&reply)))
			break;

		string str(static_cast<char*>(reply.data()), reply.size());

		sync([&]() {
			cout << "peering2: Client #" << num << ": Received '" << str << "'." << endl;
		});
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
bool send_frames(zmq::socket_t& socket, const deque<string>& frames) {
	auto penultimate = frames.end() - 1;

	for_each(frames.begin(), penultimate, [&](const string& f) {
		zmq::message_t message(f.size());
		memcpy(message.data(), f.c_str(), f.size());
		socket.send(message, ZMQ_SNDMORE);
	});

	auto last_str(frames.back());
	zmq::message_t last_frame(last_str.size());

	memcpy(last_frame.data(), last_str.c_str(), last_str.size());
	socket.send(last_frame);

	return true;
}

void print_frames(const deque<string>& frames, const string& hint) {
	sync([&]() {
		cout << "peering2: " << hint << endl;

		for (size_t i = 0; i < frames.size(); ++i)
			cout << "peering2: Frame #" << i << ":\t'" << frames[i] << "'." << endl;
	});
}

inline void sync(function<void()> fn) {
	unique_lock<decltype(mut)> lock(mut);
	fn();
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
