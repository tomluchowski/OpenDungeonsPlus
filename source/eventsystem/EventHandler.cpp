#include <eventsystem/EventHandler.h>

NotifyAction EventHandler::onNotify(Subject& subject, Event const& event)
{
    auto it = handlers.find(std::type_index(typeid(event)));
    if (it != handlers.end())
    { 
        auto range = it->second.equal_range(event.mVectorOfParameters);
        for (auto f = range.first; f != range.second; ++f)
            f->second(subject, event, f->first);
    }
    return NotifyAction::Done;
}
