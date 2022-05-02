#ifndef ECHO_SERVER_SIMPLE_THREAD_SAFE_QUEUE_H
#define ECHO_SERVER_SIMPLE_THREAD_SAFE_QUEUE_H

#include <deque>
#include <mutex>
#include <condition_variable>
#include <vector>


template<typename T>
class t_queue {
private:
    std::deque <T> queue;
    mutable std::mutex mut;
    std::condition_variable data_published_notify;
    std::condition_variable data_received_notify;
    size_t max_size = 0;

public:
    t_queue() = default;

    explicit t_queue(size_t max_size) : max_size(max_size) {}

    ~t_queue() = default;

    t_queue(const t_queue &q) = delete;

    const t_queue &operator=(const t_queue &q) = delete;

    void emplace_back(T &&d);

    void emplace_front(T &&d);

    void emplace_front_force(T &&d);

    T pop_front();

    std::vector <T> pop_front_n(uint8_t n);

    std::vector <T> pop_back_n(uint8_t n);

    T pop_back();

    [[nodiscard]] size_t get_size() const;

    [[nodiscard]] size_t get_max_size() const;
};

#endif //ECHO_SERVER_SIMPLE_THREAD_SAFE_QUEUE_H
