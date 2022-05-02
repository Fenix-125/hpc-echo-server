//
// Created by fenix on 02.05.2022.
//

#ifndef ECHO_SERVER_SIMPLE_THREAD_SAFE_RADIO_QUEUE_H
#define ECHO_SERVER_SIMPLE_THREAD_SAFE_RADIO_QUEUE_H

#include <cstdint>
#include <atomic>

#include "common/thread_safe_queue.h"


template<typename T>
class t_queue_radio : public t_queue<T> {
private:
    std::atomic_bool alive = true;
    std::atomic_uint32_t publishers = 0;
    std::atomic_uint32_t subscribers = 0;

public:
    explicit t_queue_radio(size_t max_size = 0) : t_queue<T>(max_size) {}

    ~t_queue_radio() = default;

    t_queue_radio(const t_queue_radio &q) = delete;

    const t_queue_radio &operator=(const t_queue_radio &q) = delete;

    void subscribe();

    void unsubscribe();

    [[nodiscard]] uint32_t get_sub_num() const;

    // not valid to use after all threads were unpublished
    void publish();

    // if poison pill was send is not valid to use
    bool unpublish(bool send_poison_if_last = true, T poison_data = T{});

    [[nodiscard]] bool is_alive() const;

    [[nodiscard]] uint32_t get_pub_num() const;
};

#endif //ECHO_SERVER_SIMPLE_THREAD_SAFE_RADIO_QUEUE_H
