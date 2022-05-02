#include "common/thread_safe_radio_queue.h"


template<typename T>
void t_queue_radio<T>::subscribe() {
    subscribers += 1;
}

template<typename T>
void t_queue_radio<T>::unsubscribe() {
    subscribers -= 1;
}

template<typename T>
uint32_t t_queue_radio<T>::get_sub_num() const {
    return subscribers;
}

template<typename T>
void t_queue_radio<T>::publish() {
    publishers += 1;
}

template<typename T>
bool t_queue_radio<T>::unpublish(bool send_poison_if_last, T poison_data) {
    if (publishers == 1 && send_poison_if_last) {
        t_queue<T>::emplace_back(std::move(poison_data));
        alive = false;
    }
    publishers -= 1;
}

template<typename T>
bool t_queue_radio<T>::is_alive() const {
    return alive;
}

template<typename T>
uint32_t t_queue_radio<T>::get_pub_num() const {
    return publishers;
}
