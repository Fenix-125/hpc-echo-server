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

    void emplace_back_force(T &&d);

    T pop_front();

    std::vector<T> pop_front_n(uint8_t n);

    std::vector<T> pop_back_n(uint8_t n);

    T pop_back();

    [[nodiscard]] size_t get_size() const;

    [[nodiscard]] size_t get_max_size() const;
};

template<typename T>
void t_queue<T>::emplace_back(T &&d) {
    {
        std::unique_lock <std::mutex> lg(mut);
        data_received_notify.wait(lg, [this]() { return queue.size() + 1 <= max_size || max_size == 0; });
        queue.emplace_back(std::move(d));
    }
    data_published_notify.notify_one();
}

template<typename T>
void t_queue<T>::emplace_front(T &&d) {
    {
        std::unique_lock <std::mutex> lg(mut);
        data_received_notify.wait(lg, [this]() { return queue.size() + 1 <= max_size || max_size == 0; });
        queue.emplace_front(std::move(d));
    }
    data_published_notify.notify_one();
}

template<typename T>
void t_queue<T>::emplace_front_force(T &&d) {
    {
        std::unique_lock<std::mutex> lg(mut);
        queue.emplace_front(std::move(d));
    }
    data_published_notify.notify_one();
}

template<typename T>
void t_queue<T>::emplace_back_force(T &&d) {
    {
        std::unique_lock<std::mutex> lg(mut);
        queue.emplace_back(std::move(d));
    }
    data_published_notify.notify_one();
}

template<typename T>
T t_queue<T>::pop_front() {
    T d;
    {
        std::unique_lock<std::mutex> lg(mut);
        data_published_notify.wait(lg, [this]() { return !queue.empty(); });
        d = queue.front();
        queue.pop_front();
    }
    data_received_notify.notify_one();
    return d;
}

template<typename T>
std::vector <T> t_queue<T>::pop_front_n(uint8_t n) {
    std::vector <T> res{};
    {
        std::unique_lock <std::mutex> lg(mut);
        data_published_notify.wait(lg, [this, n]() { return queue.size() >= n; });
        for (uint8_t i = 0; i < n; ++i) {
            res.emplace(res.begin() + i, std::move(queue.front()));
            queue.pop_front();
        }
    }
    data_received_notify.notify_all();
    return res;
}

template<typename T>
std::vector <T> t_queue<T>::pop_back_n(uint8_t n) {
    std::vector <T> res(n);
    {
        std::unique_lock <std::mutex> lg(mut);
        data_published_notify.wait(lg, [this, n]() { return queue.size() >= n; });
        for (uint8_t i = 0; i < n; --i) {
            res.emplace(n - 1 - i, std::move(queue.back()));
            queue.pop_back();
        }
    }
    data_received_notify.notify_all();
    return res;
}

template<typename T>
T t_queue<T>::pop_back() {
    T d;
    {
        std::unique_lock <std::mutex> lg(mut);
        data_published_notify.wait(lg, [this]() { return queue.size() != 0; });
        d = queue.back();
        queue.pop_back();
    }
    data_received_notify.notify_one();
    return d;
}

template<typename T>
size_t t_queue<T>::get_size() const {
    return queue.size();
}

template<typename T>
size_t t_queue<T>::get_max_size() const {
    return max_size;
}

#endif //ECHO_SERVER_SIMPLE_THREAD_SAFE_QUEUE_H
