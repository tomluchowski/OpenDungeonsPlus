#include "Subject.h"
#include "Observer.h"
#include "Event.h"

void Subject::registerObserver(Observer& observer) {
    if (std::find(observers.begin(), observers.end(), &observer) != observers.end())
        throw std::runtime_error("Observer already registered");
    observers.push_back(&observer);
}
void Subject::unregisterObserver(Observer& observer) {
    observers.erase(std::remove(observers.begin(), observers.end(), &observer), observers.end());
}
void Subject::notifyObservers(Event const& event) {
    std::vector<Observer*> dead;
    for (auto* o: observers)
        if (o->onNotify(*this, event) == NotifyAction::UnRegister)
            dead.push_back(o);
    auto endIt = observers.end();
    for (auto* d: dead)
        endIt = std::remove(observers.begin(), endIt, d);
    observers.erase(endIt, observers.end());
}
