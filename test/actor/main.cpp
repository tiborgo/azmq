/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#include <azmq/actor.hpp>
#include <azmq/util/scope_guard.hpp>

#include <asio/buffer.hpp>

#include <array>
#include <thread>
#include <iostream>

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

std::array<asio::const_buffer, 2> snd_bufs = {{
    asio::buffer("A"),
    asio::buffer("B")
}};

std::string subj(const char* name) {
    return std::string("inproc://") + name;
}

TEST_CASE( "Async Send/Receive", "[actor]" ) {
    asio::error_code ecc;
    size_t btc = 0;

    asio::error_code ecb;
    size_t btb = 0;
    {
        std::array<char, 2> a;
        std::array<char, 2> b;

        std::array<asio::mutable_buffer, 2> rcv_bufs = {{
            asio::buffer(a),
            asio::buffer(b)
        }};

        asio::io_service ios;
        auto s = azmq::actor::spawn(ios, [&](azmq::socket & ss) {
            ss.async_receive(rcv_bufs, [&](asio::error_code const& ec, size_t bytes_transferred) {
                ecb = ec;
                btb = bytes_transferred;
                ios.stop();
            });
            ss.get_io_service().run();
        });

        s.async_send(snd_bufs, [&] (asio::error_code const& ec, size_t bytes_transferred) {
            ecc = ec;
            btc = bytes_transferred;
        });

        asio::io_service::work w(ios);
        ios.run();
    }

    REQUIRE(ecc == asio::error_code());
    REQUIRE(btc == 4);
    REQUIRE(ecb == asio::error_code());
    REQUIRE(btb == 4);
}
