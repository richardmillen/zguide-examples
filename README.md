# zguide-examples

Implementations of the 0MQ examples in [the guide](http://zguide.zeromq.org/page:all) *([also on github](https://github.com/booksbyus/zguide))*.

The examples in this repo make extensive use of [Boost](http://www.boost.org/) and the C++ Standard Library.

The C++ source files live in their own directories. For instance, all the code for the first example [ask and ye shall receive](http://zguide.zeromq.org/page:all#Ask-and-Ye-Shall-Receive) is in `.\ask-and-ye-shall-receive`.

## Why Does This Repository Exist?

> It's a learning tool.

I implemented the code as I went through [the guide](http://zguide.zeromq.org/page:all).

+ It gave me a chance to explore other useful, but unrelated areas e.g. the use of `boost::program_options` for reading command line arguments.
+ It addresses some of the issues I had with the C++ examples in the guide *(partly because they target Linux)*
+ I've aimed to write 'Modern C++' wherever I knew (or could easily find out) how.

## Requirements

+ [Boost 1.64.0](http://www.boost.org/users/history/version_1_64_0.html) *(or you can 'recursive' clone [the repo](https://github.com/boostorg/boost))*
+ [libzmq](https://github.com/zeromq/libzmq)
+ [cppzmq](https://github.com/zeromq/cppzmq)

## Building on Windows

I used Visual Studio 2015 for the majority of the work done so far. 

All the solution files are in the `msvc` directory e.g. `.\msvc\ask-and-ye-shall-receive`, where each project uses the code files described above.

Define the following environment variables:-

| **Variable Name** | **Example Value** |
| :---------------- | :---------------- |
| BOOST_ROOT | `C:\Users\rich\code\ |
| LIBZMQ_ROOT |  |
| CPPZMQ_ROOT |  |

**IMPORTANT:** The DevStudio solutions are only setup for *Debug x86* builds.

## Building on 'nix

I hope to enable building on Mac & Linux environments soon *(probably)* using [Make](https://www.gnu.org/software/make/).
