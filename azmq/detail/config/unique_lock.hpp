/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQ_DETAIL_CONFIG_UNIQUE_LOCK_HPP_
#define AZMQ_DETAIL_CONFIG_UNIQUE_LOCK_HPP_

#include <mutex>
#define AZMQ_HAS_STD_UNIQUE_LOCK 1
namespace azmq { namespace detail {
    template<typename T>
    using unique_lock_t = std::unique_lock<T>;
} }

#endif // !defined(AZMQ_HAS_STD_UNIQUE_LOCK)



