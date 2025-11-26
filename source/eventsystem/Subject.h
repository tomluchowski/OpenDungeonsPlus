#pragma once
#include <vector>
#include <algorithm>
#include <stdexcept>
#include "Observer.h"
class Event;

class Subject {
public:
    void registerObserver(Observer& observer);
    void unregisterObserver(Observer& observer);
    void notifyObservers(Event const& event);
    virtual ~Subject(){};
private:
    std::vector<Observer*> observers;
};
