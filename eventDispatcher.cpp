#include "eventDispatcher.h"

void Dispatcher::addListener(std::string eventType, Callback callback, double weigth = 0.1) {
    this->listeners.insert({eventType, std::make_pair(callback, weigth)});
}

// notifica/trigga todos os listeners de um evento específico
void Dispatcher::notify(Event* e) {
    auto listeners = this->orderListeners(e->getType());

    for(auto func : listeners) {
        func(e);
    }
}

void Dispatcher::notify(std::string eventType) {
    auto listeners = this->orderListeners(eventType);

    for(auto func : listeners) {
        func(nullptr);
    }
}


// ordena os listeners por peso
std::vector<Callback> Dispatcher::orderListeners(std::string eventType) {
    std::vector<listenerPair> sortedArray;

    for(auto& element : this->listeners) {
        if(element.first == eventType) {
            sortedArray.push_back(element.second);
        }
    }

    sort(sortedArray.begin(), sortedArray.end(), [](listenerPair a, listenerPair b) {
        return a.second > b.second;
    });

    std::vector<Callback> res;

    std::for_each(sortedArray.begin(), sortedArray.end(), [&res](listenerPair element) {
        res.push_back(element.first);
    });

    return res;
}