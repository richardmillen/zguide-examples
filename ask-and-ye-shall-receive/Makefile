
#
# build for unix-type system
#

all: clean
	g++ hwclient.cpp /usr/local/lib/libzmq.a -o hwclient -std=c++11
	g++ hwserver.cpp /usr/local/lib/libzmq.a -o hwserver -std=c++11

clean:
	rm hwclient
	rm hwserver
