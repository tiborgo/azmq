/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQ_MESSAGE_HPP__
#define AZMQ_MESSAGE_HPP__

#include "error.hpp"
#include "util/scope_guard.hpp"

#include <cassert>
#include <asio/buffer.hpp>
#include <asio/buffers_iterator.hpp>
#include <asio/system_error.hpp>

#include <zmq.h>

#include <type_traits>
#include <memory>
#include <vector>
#include <ostream>
#include <cstring>


namespace azmq {
namespace detail {
    struct socket_ops;
}

AZMQ_V1_INLINE_NAMESPACE_BEGIN

    struct nocopy_t {};

    constexpr nocopy_t nocopy = nocopy_t{};

    struct message {
        typedef void (free_fn) (void *data);

        using flags_type = int;

        message() noexcept {
            auto rc = zmq_msg_init(&msg_);
            assert((rc == 0)&&("zmq_msg_init return non-zero")); (void)rc;
        }

        explicit message(size_t size) {
            auto rc = zmq_msg_init_size(&msg_, size);
            if (rc)
                throw asio::system_error(make_error_code());
        }

        message(asio::const_buffer const& buffer) {
            auto sz = asio::buffer_size(buffer);
            auto rc = zmq_msg_init_size(&msg_, sz);
            if (rc)
                throw asio::system_error(make_error_code());
            asio::buffer_copy(asio::buffer(zmq_msg_data(&msg_), sz),
                                     buffer);
        }

        message(nocopy_t, asio::const_buffer const& buffer)
            : message(nocopy,
                asio::mutable_buffer(
                    (void *)asio::buffer_cast<const void*>(buffer),
                    asio::buffer_size(buffer)),
                nullptr,
                nullptr)
        {}

        message(nocopy_t, asio::mutable_buffer const& buffer, void* hint, zmq_free_fn* deleter)
        {
            auto rc = zmq_msg_init_data(&msg_,
                                        asio::buffer_cast<void*>(buffer),
                                        asio::buffer_size(buffer),
                                        deleter, hint);
            if (rc)
                throw asio::system_error(make_error_code());
        }

        template<class Deleter, class Enabler =
            typename std::enable_if<!std::is_convertible<Deleter, free_fn *>::value>::type
        >
        message(nocopy_t, asio::mutable_buffer const& buffer, Deleter&& deleter)
        {
            using D = typename std::decay<Deleter>::type;

            const auto call_deleter = [](void *buf, void *hint) {
                std::unique_ptr<D> deleter(reinterpret_cast<D*>(hint));
                assert((deleter)&&("!deleter"));
                (*deleter)(buf);
            };

            std::unique_ptr<D> d(new D(std::forward<Deleter>(deleter)));
            auto rc = zmq_msg_init_data(&msg_,
                                        asio::buffer_cast<void*>(buffer),
                                        asio::buffer_size(buffer),
                                        call_deleter, d.get());
            if (rc)
                throw asio::system_error(make_error_code());
            d.release();
        }

        message(nocopy_t, asio::mutable_buffer const& buffer, free_fn* deleter)
        {
            assert((deleter)&&("!deleter"));

            const auto call_deleter = [](void *buf, void *hint) {
                free_fn *deleter(reinterpret_cast<free_fn*>(hint));
                (*deleter)(buf);
            };

            auto rc = zmq_msg_init_data(&msg_,
                                        asio::buffer_cast<void*>(buffer),
                                        asio::buffer_size(buffer),
                                        call_deleter, reinterpret_cast<void *>(deleter));
            if (rc)
                throw asio::system_error(make_error_code());
        }

        explicit message(const std::string& str)
            : message(asio::buffer(str.data(), str.size()))
        { }

        message(message && rhs) noexcept
            : msg_(rhs.msg_)
        {
            auto rc = zmq_msg_init(&rhs.msg_);
            assert((rc == 0)&&("zmq_msg_init return non-zero")); (void)rc;
        }

        message& operator=(message && rhs) noexcept {
            msg_ = rhs.msg_;
            auto rc = zmq_msg_init(&rhs.msg_);
            assert((rc == 0)&&("zmq_msg_init return non-zero")); (void)rc;

            return *this;
        }

        message(message const& rhs) {
            auto rc = zmq_msg_init(const_cast<zmq_msg_t*>(&msg_));
            assert((rc == 0)&&("zmq_msg_init return non-zero"));
            rc = zmq_msg_copy(const_cast<zmq_msg_t*>(&msg_),
                              const_cast<zmq_msg_t*>(&rhs.msg_));
            if (rc)
                throw asio::system_error(make_error_code());
        }

        message& operator=(message const& rhs) {
            auto rc = zmq_msg_copy(const_cast<zmq_msg_t*>(&msg_),
                                   const_cast<zmq_msg_t*>(&rhs.msg_));
            if (rc)
                throw asio::system_error(make_error_code());
            return *this;
        }

        ~message() { close(); }

        asio::const_buffer cbuffer() const noexcept {
            return asio::buffer(data(), size());
        }

        operator asio::const_buffer() const noexcept {
            return cbuffer();
        }

        asio::const_buffer buffer() const noexcept {
            return cbuffer();
        }

        asio::mutable_buffer buffer() {
            if (is_shared())
                deep_copy();

            return asio::buffer(const_cast<void *>(data()), size());
        }

        template<typename T>
        T const& buffer_cast() const {
            return *asio::buffer_cast<T const*>(buffer());
        }

        size_t buffer_copy(asio::mutable_buffer const& target) const {
            return asio::buffer_copy(target, buffer());
        }

        bool operator==(const message & rhs) const noexcept {
            return !operator!=(rhs);
        }

        bool operator!=(const message & rhs) const noexcept {
            const auto sz = size();

            return sz != rhs.size()
                || 0 != std::memcmp(data(), rhs.data(), sz);
        }

        std::string string() const {
            return std::string(static_cast<const char *>(data()), size());
        }

        const void *data() const noexcept {
            return zmq_msg_data(const_cast<zmq_msg_t*>(&msg_));
        }

        size_t size() const noexcept {
            return zmq_msg_size(const_cast<zmq_msg_t*>(&msg_));
        }

        bool more() const noexcept {
            return zmq_msg_more(const_cast<zmq_msg_t*>(&msg_)) ? true : false;
        }

    private:
        friend detail::socket_ops;
        zmq_msg_t msg_;

        void close() noexcept {
            auto rc = zmq_msg_close(&msg_);
            assert((rc == 0)&&("zmq_msg_close return non-zero")); (void)rc;
        }

        // note, this is a bit fragile, currently the last two bytes in a
        // zmq_msg_t hold the type and flags fields
        enum {
            flags_offset = sizeof(zmq_msg_t) - 1,
            type_offset = sizeof(zmq_msg_t) - 2
        };

        uint8_t flags() const noexcept {
            auto pm = const_cast<zmq_msg_t*>(&msg_);
            auto p = reinterpret_cast<uint8_t*>(pm);
            return p[flags_offset];
        }

        uint8_t type() const noexcept {
            auto pm = const_cast<zmq_msg_t*>(&msg_);
            auto p = reinterpret_cast<uint8_t*>(pm);
            return p[type_offset];
        }

        // flags and type we care about from msg.hpp in zeromq lib
        enum
        {
            flag_shared = 128,
            type_cmsg = 104
        };

        bool is_shared() const noexcept {
            // TODO use shared message property if libzmq version >= 4.1
            return (flags() & flag_shared) || type() == type_cmsg;
        }

        void deep_copy() {
            auto sz = size();
            zmq_msg_t tmp;
            auto rc = zmq_msg_init(&tmp);
            assert((rc == 0)&&("zmq_msg_init return non-zero"));

            rc = zmq_msg_move(&tmp, &msg_);
            assert((rc == 0)&&("zmq_msg_move return non-zero"));

            // ensure that tmp is always cleaned up
            SCOPE_EXIT { zmq_msg_close(&tmp); };

            rc = zmq_msg_init_size(&msg_, sz);
            if (rc)
                throw asio::system_error(make_error_code());

            auto pdst = zmq_msg_data(const_cast<zmq_msg_t*>(&msg_));
            auto psrc = zmq_msg_data(&tmp);
            ::memcpy(pdst, psrc, sz);
        }
    };

    using message_vector = std::vector<message>;

    template<typename BufferSequence>
    message_vector to_message_vector(BufferSequence const& buffers) {
        return message_vector(std::begin(buffers), std::end(buffers));
    }
AZMQ_V1_INLINE_NAMESPACE_END
} // namespace azmq
#endif // AZMQ_MESSAGE_HPP__
