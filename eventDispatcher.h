#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <algorithm>

class Event {
public:
    Event() {

    }

    std::string getType() const {
        return this->eventType;
    }

protected:
    std::string eventType;
};

using Callback      = std::function<void(Event*)>;
using listenerPair  = std::pair<Callback, double>;

class Dispatcher {
public:
    void addListener(std::string eventType, Callback callback, double weigth = 0.1);

    void notify(Event* e);
    void notify(std::string eventType);

private:
    std::unordered_multimap<std::string, listenerPair> listeners;

    // ordena os listeners por peso
    std::vector<Callback> orderListeners(std::string eventType);
};