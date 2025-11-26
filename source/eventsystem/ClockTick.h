#ifndef CLOCK_TICK_H
#define CLOCK_TICK_H

#include <vector>

#include "Event.h"

class ClockTick : public Event {
public:
    ClockTick(unsigned int mm, unsigned int ss) { mVectorOfParameters.push_back(mm); mVectorOfParameters.push_back(ss);}
};

#endif // CLOCK_TICK_H
