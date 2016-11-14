*(This is fork of [AZMQ Boost Asio + ZeroMQ](https://github.com/zeromq/azmq) with [Boost](http://www.boost.org/) removed.)*

# AZMQ Asio + ZeroMQ

## Welcome
The azmq library provides Asio style bindings for ZeroMQ

This library is built on top of ZeroMQ's standard C interface and is
intended to work well with C++ applications which use the Asio library.

The main abstraction exposed by the library is azmq::socket which
provides an Asio style socket interface to the underlying zeromq socket
and interfaces with Asio's io_service().  The socket implementation
participates in the io_service's reactor for asynchronous IO and
may be freely mixed with other Asio socket types (raw TCP/UDP/Serial/etc.).

## Building and installation

Building requires a recent version of CMake (2.8.12 or later for Visual Studio, 2.8 or later for the rest), and a C++ compiler
which supports C++11. Currently this has been tested with -
* Xcode 8.1 on OS X 10.11

Library dependencies are -
* [Asio](http://think-async.com/) 1.10.8 or later
* [ZeroMQ](http://zeromq.org/) 4.0.x

To build on Linux / OS X -
```
$ mkdir build && cd build
$ cmake ..
$ make
$ make test
$ make install
```

To build on Windows -
```
> mkdir build
> cd build
> cmake ..
> cmake --build . --config Release
> ctest . -C Release
```
You can also open Visual Studio solution from `build` directory after invoking CMake.

To change the default install location use `-DCMAKE_INSTALL_PREFIX` when invoking CMake.

To change where the build looks for Asio and ZeroMQ use `-DASIO_ROOT=<my custom Asio install>` and `-DZMQ_ROOT=<my custom ZeroMQ install>` when invoking CMake. Or set `ASIO_ROOT` and `ZMQ_ROOT` environment variables.

## Example Code
This is an azmq version of the code presented in the ZeroMQ guide at
http://zeromq.org/intro:read-the-manual

```cpp
#include <azmq/socket.hpp>
#include <asio.hpp>
#include <array>

namespace asio = asio;

int main(int argc, char** argv) {
    asio::io_service ios;
    azmq::sub_socket subscriber(ios);
    subscriber.connect("tcp://192.168.55.112:5556");
    subscriber.connect("tcp://192.168.55.201:7721");
    subscriber.set_option(azmq::socket::subscribe("NASDAQ"));

    azmq::pub_socket publisher(ios);
    publisher.bind("ipc://nasdaq-feed");

    std::array<char, 256> buf;
    for (;;) {
        auto size = subscriber.receive(asio::buffer(buf));
        publisher.send(asio::buffer(buf));
    }
    return 0;
}
```

Further examples may be found in doc/examples

## Copying

Use of this software is granted under the the BOOST 1.0 license
(same as Boost Asio).  For details see the file `LICENSE-BOOST_1_0
included with the distribution.

## Contributing

AZMQ uses the [C4.1 (Collective Code Construction Contract)](http://rfc.zeromq.org/spec:22) process for contributions.
See the accompanying CONTRIBUTING file for more information.
