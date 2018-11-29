/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#pragma once

#include <thread>
#include <atomic>
#include <vector>
#include <boost/noncopyable.hpp>
#include <fc/log/logger.hpp>

#include "consumer_core.h"
#include "fifo.h"

namespace eosio {

template<typename T>
class consumer final : public boost::noncopyable {
public:
    consumer(std::unique_ptr<consumer_core<T> > core);
    ~consumer();

    void add_consumer_thread(std::unique_ptr<consumer_core<T>> core);
    void push(const T& element);

private:
    void run(std::unique_ptr<consumer_core<T> >& core);

    fifo<T> m_fifo;
    std::atomic<bool> m_exit;
    std::vector<std::unique_ptr<std::thread>> m_thread_pool;
};

template<typename T>
consumer<T>::consumer(std::unique_ptr<consumer_core<T> > core):
    m_fifo(fifo<T>::behavior::blocking),
    m_exit(false) {
    m_thread_pool.push_back(std::make_unique<std::thread>([&]{this->run(core);}));
}

template<typename T>
consumer<T>::~consumer() {
    m_fifo.set_behavior(fifo<T>::behavior::not_blocking);
    m_exit = true;
    for (auto& thread : m_thread_pool) {
        thread->join();
    }
}
template<typename T>
void consumer<T>::add_consumer_thread(std::unique_ptr<consumer_core<T>> core) {
    m_thread_pool.push_back(std::make_unique<std::thread>([&]{this->run(core);}));
}

template<typename T>
void consumer<T>::push(const T& element) {
    m_fifo.push(element);
}

template<typename T>
void consumer<T>::run(std::unique_ptr<consumer_core<T>>& core) {
    dlog("Consumer thread Start");
    while (true) {
        try {
            auto elements = m_fifo.pop_all();
            if (m_exit && elements.size() == 0) break;
            core->consume(elements);
        } catch (fc::exception &e) {
            elog("FC Exception while consume data: ${e}", ("e", e.to_detail_string()));
        } catch (std::exception &e) {
            elog("STD Exception while consume data: ${e}", ("e", e.what()));
        } catch (...) {
            elog("Unknown exception happen during consuming data."); // prevent crash some errors in decode.
        }
    }
    dlog("Consumer thread End");
}

} // namespace

