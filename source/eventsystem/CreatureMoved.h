#ifndef CREATUREMOVED_H
#define CREATUREMOVED_H
#include "Event.h"
#include <vector>




class CreatureMoved : public Event {
public:
        static int GAME_MAP_WIDTH ;
	static std::vector<bool> alreadyVisited;
	CreatureMoved(int x , int y ) {mVectorOfParameters.push_back(x); mVectorOfParameters.push_back(y);}
};

#endif //CREATUREMOVED_H
