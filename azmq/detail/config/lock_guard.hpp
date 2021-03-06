/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQ_DETAIL_CONFIG_LOCK_GUARD_HPP_
#define AZMQ_DETAIL_CONFIG_LOCK_GUARD_HPP_

#include <mutex>
#define AZMQ_HAS_STD_LOCK_GUARD 1
namespace azmq { namespace detail {
    template<typename T>
    using lock_guard_t = std::lock_guard<T>;
} }

#endif // AZMQ_DETAIL_CONFIG_LOCK_GUARD_HPP_


