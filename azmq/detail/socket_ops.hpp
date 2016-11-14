/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQ_DETAIL_SOCKET_OPS_HPP__
#define AZMQ_DETAIL_SOCKET_OPS_HPP__

#include "../error.hpp"
#include "../message.hpp"
#include "context_ops.hpp"

#include <cassert>
#include <regex>
#include <asio/io_service.hpp>
#include <asio/socket_base.hpp>
#if ! defined ASIO_WINDOWS
    #include <asio/posix/stream_descriptor.hpp>
#else
    #include <asio/ip/tcp.hpp>
#endif
#include <system_error>
#include <random>

#include <zmq.h>

#include <iterator>
#include <memory>
#include <string>
#include <sstream>
#include <type_traits>

namespace azmq {
namespace detail {
    template <typename T>
    class has_begin {
    private:
        typedef char Yes;
        typedef Yes No[2];
        
        template<typename C> static auto Test(void*)
        -> decltype(typename C::const_iterator{std::declval<C const>().begin()}, Yes{});
        
        template<typename> static No& Test(...);
        
    public:
        static bool const value = sizeof(Test<T>(0)) == sizeof(Yes);
    };
    
    struct socket_ops {
        using endpoint_type = std::string;

        struct socket_close {
            void operator()(void* socket) {
                int v = 0;
                auto rc = zmq_setsockopt(socket, ZMQ_LINGER, &v, sizeof(int));
                assert((rc == 0)&&("set linger=0 on shutdown")); (void)rc;
                zmq_close(socket);
            }
        };

        enum class dynamic_port : uint16_t {
            first = 0xc000,
            last = 0xffff
        };

        using raw_socket_type = void*;
        using socket_type = std::unique_ptr<void, socket_close>;

#if ! defined ASIO_WINDOWS
        using posix_sd_type = asio::posix::stream_descriptor;
        using native_handle_type = asio::posix::stream_descriptor::native_handle_type;
        struct stream_descriptor_close {
            void operator()(posix_sd_type* sd) {
                sd->release();
                delete sd;
            }
        };
        using stream_descriptor = std::unique_ptr<posix_sd_type, stream_descriptor_close>;
#else
        using native_handle_type = asio::ip::tcp::socket::native_handle_type;
        using stream_descriptor = std::unique_ptr<asio::ip::tcp::socket>;
#endif
        using flags_type = message::flags_type;
        using more_result_type = std::pair<size_t, bool>;

        static socket_type create_socket(context_ops::context_type context,
                                         int type,
                                         asio::error_code & ec) {
            assert((context)&&("Invalid context"));
            auto res = zmq_socket(context.get(), type);
            if (!res) {
                ec = make_error_code();
                return socket_type();
            }
            return socket_type(res);
        }

        static stream_descriptor get_stream_descriptor(asio::io_service & io_service,
                                                       socket_type & socket,
                                                       asio::error_code & ec) {
            assert((socket)&&("invalid socket"));
            native_handle_type handle = 0;
            auto size = sizeof(native_handle_type);
            stream_descriptor res;
            auto rc = zmq_getsockopt(socket.get(), ZMQ_FD, &handle, &size);
            if (rc < 0)
                ec = make_error_code();
            else {
#if ! defined ASIO_WINDOWS
                res.reset(new asio::posix::stream_descriptor(io_service, handle));
#else
                // Use duplicated SOCKET, because ASIO socket takes ownership over it so destroys one in dtor.
                ::WSAPROTOCOL_INFO pi;
                ::WSADuplicateSocket(handle, ::GetCurrentProcessId(), &pi);
                handle = ::WSASocket(pi.iAddressFamily/*AF_INET*/, pi.iSocketType/*SOCK_STREAM*/, pi.iProtocol/*IPPROTO_TCP*/, &pi, 0, 0);
                res.reset(new asio::ip::tcp::socket(io_service, asio::ip::tcp::v4(), handle));
#endif
            }
            return res;
        }

        static asio::error_code cancel_stream_descriptor(stream_descriptor & sd,
                                                                  asio::error_code & ec) {
            assert((sd)&&("invalid stream_descriptor"));
            return sd->cancel(ec);
        }

        static asio::error_code bind(socket_type & socket,
                                              endpoint_type & ep,
                                              asio::error_code & ec) {
            assert((socket)&&("invalid socket"));
            const std::regex simple_tcp("^tcp://.*:(\\d+)$");
            const std::regex dynamic_tcp("^(tcp://.*):([*!])(\\[(\\d+)?-(\\d+)?\\])?$");
            std::smatch mres;
            int rc = -1;
            if (std::regex_match(ep, mres, simple_tcp)) {
                if (zmq_bind(socket.get(), ep.c_str()) == 0)
                    rc = std::stoi(mres.str(1));
            } else if (std::regex_match(ep, mres, dynamic_tcp)) {
                auto const& hostname = mres.str(1);
                auto const& opcode = mres.str(2);
                auto const& first_str = mres.str(4);
                auto const& last_str = mres.str(5);
                
                if ((!first_str.empty() && std::stoi(first_str) > UINT16_MAX) ||
                    (!last_str.empty() && std::stoi(last_str) > UINT16_MAX)) {
                    throw std::bad_cast();
                }
                
                auto first = first_str.empty() ? static_cast<uint16_t>(dynamic_port::first)
                                               : (uint16_t)std::stoi(first_str);
                auto last = last_str.empty() ? static_cast<uint16_t>(dynamic_port::last)
                                             : (uint16_t)std::stoi(last_str);
                uint16_t port = first;
                if (opcode[0] == '!') {
                    static std::mt19937 gen;
                    std::uniform_int_distribution<> port_range(port, last);
                    port = port_range(gen);
                }
                auto attempts = last - first;
                while (rc < 0 && attempts--) {
                    ep = hostname + ":" + std::to_string(port);
                    if (zmq_bind(socket.get(), ep.c_str()) == 0)
                        rc = port;
                    if (++port > last)
                        port = first;
                }
            } else {
                rc = zmq_bind(socket.get(), ep.c_str());
            }
            if (rc < 0)
                ec = make_error_code();
            return ec;
        }

        static asio::error_code unbind(socket_type & socket,
                                                endpoint_type const& ep,
                                                asio::error_code & ec) {
            assert((socket)&&("invalid socket"));
            auto rc = zmq_unbind(socket.get(), ep.c_str());
            if (rc < 0)
                ec = make_error_code();
            return ec;
        }

        static asio::error_code connect(socket_type & socket,
                                                 endpoint_type const& ep,
                                                 asio::error_code & ec) {
            assert((socket)&&("invalid socket"));
            auto rc = zmq_connect(socket.get(), ep.c_str());
            if (rc < 0)
                ec = make_error_code();
            return ec;
        }

        static asio::error_code disconnect(socket_type & socket,
                                                    endpoint_type const& ep,
                                                    asio::error_code & ec) {
            assert((socket)&&("invalid socket"));
            auto rc = zmq_disconnect(socket.get(), ep.c_str());
            if (rc < 0)
                ec = make_error_code();
            return ec;
        }

        template<typename Option>
        static asio::error_code set_option(socket_type & socket,
                                                    Option const& opt,
                                                    asio::error_code & ec) {
            auto rc = zmq_setsockopt(socket.get(), opt.name(), opt.data(), opt.size());
            if (rc < 0)
                ec = make_error_code();
            return ec;
        }

        template<typename Option>
        static asio::error_code get_option(socket_type & socket,
                                                    Option & opt,
                                                    asio::error_code & ec) {
            assert((socket)&&("invalid socket"));
            size_t size = opt.size();
            auto rc = zmq_getsockopt(socket.get(), opt.name(), opt.data(), &size);
            if (rc < 0)
                ec = make_error_code();
            return ec;
        }

        static int get_events(socket_type & socket,
                              asio::error_code & ec) {
            assert((socket)&&("invalid socket"));
            int evs = 0;
            size_t size = sizeof(evs);
            auto rc = zmq_getsockopt(socket.get(), ZMQ_EVENTS, &evs, &size);
            if (rc < 0) {
                ec = make_error_code();
                return 0;
            }
            return evs;
        }

        static int get_socket_kind(socket_type & socket,
                                   asio::error_code & ec) {
            assert((socket)&&("invalid socket"));
            int kind = 0;
            size_t size = sizeof(kind);
            auto rc = zmq_getsockopt(socket.get(), ZMQ_TYPE, &kind, &size);
            if (rc < 0)
                ec = make_error_code();
            return kind;
        }

        static bool get_socket_rcvmore(socket_type & socket) {
            assert((socket)&&("invalid socket"));
            int more = 0;
            size_t size = sizeof(more);
            auto rc = zmq_getsockopt(socket.get(), ZMQ_RCVMORE, &more, &size);
            if (rc == 0)
                return more == 1;
            return false;
        }

        static size_t send(message const& msg,
                           socket_type & socket,
                           flags_type flags,
                           asio::error_code & ec) {
            assert((socket)&&("Invalid socket"));
            auto rc = zmq_msg_send(const_cast<zmq_msg_t*>(&msg.msg_), socket.get(), flags);
            if (rc < 0) {
                ec = make_error_code();
                return 0;
            }
            return rc;
        }

        template<typename ConstBufferSequence>
        static auto send(ConstBufferSequence const& buffers,
                         socket_type & socket,
                         flags_type flags,
                         asio::error_code & ec) ->
            typename std::enable_if<has_begin<ConstBufferSequence>::value, size_t>::type
        {
            size_t res = 0;
            auto last = std::distance(std::begin(buffers), std::end(buffers)) - 1;
            auto index = 0u;
            for (auto it = std::begin(buffers); it != std::end(buffers); ++it, ++index) {
                auto f = index == last ? flags
                                       : flags | ZMQ_SNDMORE;
                res += send(message(*it), socket, f, ec);
                if (ec) return 0u;
            }
            return res;
        }

        static size_t receive(message & msg,
                              socket_type & socket,
                              flags_type flags,
                              asio::error_code & ec) {
            assert((socket)&&("Invalid socket"));
            auto rc = zmq_msg_recv(const_cast<zmq_msg_t*>(&msg.msg_), socket.get(), flags);
            if (rc < 0) {
                ec = make_error_code();
                return 0;
            }
            return rc;
        }

        template<typename MutableBufferSequence>
        static auto receive(MutableBufferSequence const& buffers,
                            socket_type & socket,
                            flags_type flags,
                            asio::error_code & ec) ->
            typename std::enable_if<has_begin<MutableBufferSequence>::value, size_t>::type
        {
            size_t res = 0;
            message msg;
            auto it = std::begin(buffers);
            do {
                auto sz = receive(msg, socket, flags, ec);
                if (ec)
                    return 0;

                if (msg.buffer_copy(*it++) < sz) {
                    ec = make_error_code(std::errc::no_buffer_space);
                    return 0;
                }

                res += sz;
                flags |= ZMQ_RCVMORE;
            } while ((it != std::end(buffers)) && msg.more());

            if (msg.more())
                ec = make_error_code(std::errc::no_buffer_space);
            return res;
        }

        static size_t receive_more(message_vector & vec,
                                   socket_type & socket,
                                   flags_type flags,
                                   asio::error_code & ec) {
            size_t res = 0;
            message msg;
            bool more = false;
            do {
                auto sz = receive(msg, socket, flags, ec);
                if (ec)
                    return 0;
                more = msg.more();
                vec.emplace_back(std::move(msg));
                res += sz;
                flags |= ZMQ_RCVMORE;
            } while (more);
            return res;
        }

        static size_t flush(socket_type & socket,
                            asio::error_code & ec) {
            size_t res = 0;
            message msg;
            while (get_socket_rcvmore(socket)) {
                auto sz = receive(msg, socket, ZMQ_RCVMORE, ec);
                if (ec)
                    return 0;
                res += sz;
            };
            return res;
        }

        static std::string monitor(socket_type & socket,
                                   int events,
                                   asio::error_code & ec) {
            assert((socket)&&("Invalid socket"));
            std::ostringstream stm;
            stm << "inproc://monitor-" << socket.get();
            auto addr = stm.str();
            auto rc = zmq_socket_monitor(socket.get(), addr.c_str(), events);
            if (rc < 0)
                ec = make_error_code();
            return addr;
        }
    };
} // namespace detail
} // namespace azmq
#endif // AZMQ_DETAIL_SOCKET_OPS_HPP__

