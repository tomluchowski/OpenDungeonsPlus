#pragma once
#include <unordered_map>
#include <map>
#include <typeindex>
#include <functional>
#include "Observer.h"
#include "Event.h"

/*template<typename T>
struct EventConfigTraits {
    template<typename... Args>
    static std::function<void(Subject&, Event const&)> createHandler(
        std::function<void(Subject&, Event const&)> f, Args&&...) { return f; }
};
*/
class EventHandler : public Observer {
public:
    NotifyAction onNotify(Subject& subject, Event const& event) override;
    template<typename T>
    void registerEventHandler(std::function<void(Subject&, Event const&, std::vector<int>)> f, std::vector<int> mParameterPack) {
        handlers[std::type_index(typeid(T))].insert(std::make_pair(mParameterPack,f));
    }
    std::unordered_map<std::type_index, std::multimap<std::vector<int>,std::function<void(Subject&, Event const&, std::vector<int> )>>> handlers;
};
/*
template<>
struct EventConfigTraits<ClockTick> {
    template<typename... Args>
    static std::function<void(Subject&, Event const&)> createHandler(
        std::function<void(Subject&, Event const&)> f,
        std::chrono::seconds ii, std::chrono::minutes jj) {
        return [=](Subject& ss, Event const& ee){
            auto& e = dynamic_cast<const ClockTick&>(ee);
            if (e.seconds == ii && e.minutes == jj) f(ss, ee);
        };
    }
};*/
