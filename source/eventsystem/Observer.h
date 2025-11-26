#pragma once

class Subject;
class Event;

enum class NotifyAction { Done, UnRegister };

class Observer {
public:
    virtual ~Observer() {}
    virtual NotifyAction onNotify(Subject& subject, Event const& event) = 0;
};
