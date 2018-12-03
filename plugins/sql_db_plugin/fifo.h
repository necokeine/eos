/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#pragma once

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <deque>
#include <vector>
#include <boost/noncopyable.hpp>

namespace eosio {

template<typename T>
class fifo : public boost::noncopyable {
public:
    enum class behavior {blocking, not_blocking};

    fifo(behavior value);

    void push(const T& element);
    std::deque<T> pop_all();
    void set_behavior(behavior value);

private:
    std::condition_variable m_cond;
    std::atomic<behavior> m_behavior;
    std::mutex m_mux_producer;
    std::mutex m_mux_consumer;
    std::deque<T> m_producer;
    std::deque<T> m_wait_for_pick;
};

template<typename T>
fifo<T>::fifo(behavior value) {
    m_behavior = value;
}

template<typename T>
void fifo<T>::push(const T& element) {
    std::lock_guard<std::mutex> lock(m_mux_producer);
    m_producer.push_back(element);
    m_cond.notify_one();
}

template<typename T>
std::deque<T> fifo<T>::pop_all() {
    std::unique_lock<std::mutex> lock_read(m_mux_consumer);
    if (m_wait_for_pick.empty()) {
        std::unique_lock<std::mutex> lock(m_mux_producer);
        m_cond.wait(lock, [&]{return m_behavior == behavior::not_blocking || !m_producer.empty();});
        std::swap(m_wait_for_pick, m_producer);
    }

    const int size_per_pick = 10000;
    std::deque<T> result;
    result.clear();
    if (m_wait_for_pick.size() <= size_per_pick) {
        std::swap(result, m_wait_for_pick);
    } else {
        for (int i = 0; i < size_per_pick; i++) {
            result.push_back(std::move(m_wait_for_pick.front()));
            m_wait_for_pick.pop_front();
        }
    }
    return result;
}

template<typename T>
void fifo<T>::set_behavior(behavior value) {
    m_behavior = value;
    m_cond.notify_all();
}

} // namespace
