/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#pragma once

#include <vector>
#include <deque>

namespace eosio {

template<typename T>
class consumer_core {
public:
    virtual ~consumer_core() {}
    virtual void consume(const unsigned int left, const unsigned int right) = 0;
    virtual void consume(const T& element) { }
};

} // namespace


