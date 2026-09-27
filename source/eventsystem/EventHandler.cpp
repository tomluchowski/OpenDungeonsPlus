#include <eventsystem/EventHandler.h>

NotifyAction EventHandler::onNotify(Subject& subject, Event const& event)
{
    typedef std::multimap<std::vector<int>, std::function<void(Subject&, Event const&, std::vector<int>)>> HandlerMap;
    std::unordered_map<std::type_index, HandlerMap>::iterator it = handlers.find(std::type_index(typeid(event)));
    if (it != handlers.end())
    { 
        std::pair<HandlerMap::iterator, HandlerMap::iterator> range = it->second.equal_range(event.mVectorOfParameters);
        for (HandlerMap::iterator f = range.first; f != range.second; ++f)
            f->second(subject, event, f->first);
    }
    return NotifyAction::Done;
}
