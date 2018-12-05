/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#pragma once

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <utility>
#include <vector>
#include <boost/noncopyable.hpp>

namespace eosio {

template<typename T>
class fifo : public boost::noncopyable {
public:
    enum class behavior {blocking, not_blocking};

    fifo(behavior value);

    void push(const unsigned int& block_num);
    std::pair<unsigned int, unsigned int> pop_all();
    void set_behavior(behavior value);

private:
    std::condition_variable m_cond;
    std::atomic<behavior> m_behavior;
    std::mutex m_mux;
    unsigned int current_biggest_block;
    unsigned int last_fetched_block;
};

template<typename T>
fifo<T>::fifo(behavior value) {
    m_behavior = value;
    current_biggest_block = 0;
    last_fetched_block = 2;
}

template<typename T>
void fifo<T>::push(const unsigned int& block_num) {
    std::lock_guard<std::mutex> lock(m_mux);
    current_biggest_block = std::max(current_biggest_block, block_num);
    m_cond.notify_one();
}

template<typename T>
std::pair<unsigned int, unsigned int> fifo<T>::pop_all() {

    const unsigned int size_per_pick = 1000;
    unsigned int left = 0, right = 0;
    {
        std::unique_lock<std::mutex> lock(m_mux);
        m_cond.wait(lock, [&]{return m_behavior == behavior::not_blocking || (current_biggest_block >= last_fetched_block);});
        left = last_fetched_block;
        right = std::min(left + size_per_pick, current_biggest_block);
        last_fetched_block = right + 1;
    }
    return std::make_pair(left, right);
}

template<typename T>
void fifo<T>::set_behavior(behavior value) {
    m_behavior = value;
    m_cond.notify_all();
}

} // namespace
