#include "modes/ConsoleCommands.h"

#include "entities/Creature.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "goals/Goal.h"
#include "modes/ConsoleInterface.h"
#include "modes/GameEditorModeConsole.h"
#include "modes/InputManager.h"
#include "modes/ModeManager.h"
#include "modes/AbstractModeManager.h"
#include "modes/GameMode.h"
#include "network/ClientNotification.h"
#include "network/ODClient.h"
#include "network/ODServer.h"
#include "render/ODFrameListener.h"
#include "render/RenderManager.h"
#include "rooms/Room.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"


#include <CEGUI/widgets/Editbox.h>
#include <OgreCamera.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreViewport.h>
#include <OgreShadowCameraSetupLiSPSM.h>
#include <boost/algorithm/string/join.hpp>

#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <functional>

 

template<typename M, typename Ret, typename... Args>
void addCommand(M &&m, const char *name, Ret (*cmdStrFunc)(Args...)) {
	m.def(
		name, 
		[cmdStrFunc, name](Args... args) {
			GameEditorModeConsole::getSingleton().mConsoleInterface.tryExecuteClientCommand(
				std::string(name) + ' ' + cmdStrFunc(args...),
				GameEditorModeConsole::getSingletonPtr()->mModeManager->getCurrentModeType(),
				*(GameEditorModeConsole::getSingletonPtr()->mModeManager)); 
		}, 
		getDocString().at(name)
	);
}

template<typename M>
void addCommand(M &&m, const char *name) {
	addCommand(m, name, +[]() { return ""; });
}
 
 
 
 
PYBIND11_EMBEDDED_MODULE(cheats, m){
    addCommand(m, "addcreature",  +[](int seat, std::string creatureName,  std::string meshName, float x, float y, float z, std::string creatureDefinitionString, int level, int exp, std::string hpString, int wakeFullness, int hunger, int goldCarried, std::string weaponLeft, std::string weaponRight, std::string skillsList, int weaponDropDeath  ){ return Helper::toString(seat) + " " + creatureName + " " + meshName + " " +  Helper::toString(x) + " " +  Helper::toString(y) + " " +  Helper::toString(z) + " "  + creatureDefinitionString + " " + Helper::toString(level) + " " +  Helper::toString(exp) + " " + hpString + " " +  Helper::toString(wakeFullness) + " " +  Helper::toString(hunger) + " " +  Helper::toString(goldCarried) + " " + weaponLeft + " " + weaponRight + " " + skillsList + " " +  Helper::toString(weaponDropDeath) ;});    
    addCommand(m, "addgold", +[](int seat, int gold){ return Helper::toString(seat)+ " " +Helper::toString(gold); });
    addCommand(m, "addmana", +[](int seat, int mana){ return Helper::toString(seat)+ " " +Helper::toString(mana); });
    addCommand(m, "catmullspline", +[](std::vector<double> points){ return Helper::toString(points);  });    
    addCommand(m, "circlearound",  +[](double x, double y, double radious){ return Helper::toString(x) + " " +  Helper::toString(y) + " " + Helper::toString(radious);      });
    addCommand(m, "creaturevisdebug", +[](std::string subject){ return subject; });    
    addCommand(m, "farclip",+[](float distance){ return Helper::toString(distance); });
    addCommand(m, "fps", +[](int frames){ return Helper::toString(frames); });
    addCommand(m, "helpmessage");
    addCommand(m, "list", +[](std::string subject){ return subject; });
    addCommand(m, "listmeshanims", +[](std::string name){ return name;});
    addCommand(m, "logfloodfill");
    addCommand(m, "maxtime",  +[](int time){ return Helper::toString(time);});
    addCommand(m, "nearclip",+[](float distance){ return Helper::toString(distance); });
    addCommand(m, "seatvisdebug",  +[](int seat){ return Helper::toString(seat); });
    addCommand(m, "setcamerafovy",  +[](double fovy){ return Helper::toString(fovy);});
    addCommand(m, "setcreaturedest",  +[](std::string name, int x, int y){  return name +  Helper::toString(x)+ " " +Helper::toString(y);     });
    addCommand(m, "setcreaturelevel",  +[](int name){ return Helper::toString(name);});
    addCommand(m, "termwidth", +[](int width){ return Helper::toString(width); });
    addCommand(m, "togglefow");        
    addCommand(m, "triggercompositor", +[](std::string name){ return name;});
    addCommand(m, "unlockskills");
    
}





namespace
{

const std::string HELPMESSAGE =
        "The console is a way of interacting with the underlying game engine directly."
        "Commands given to the the are made up of two parts: a \'command name\' and one or more \'arguments\'."
        "For information on how to use a particular command, type help followed by the command name."
        "\n\nThe following commands are available:"
        "\n\n==General=="
        "\n\thelp - Displays this help message."
        "\n\thelp keys - Shows the keyboard controls."
        "\n\tlist/ls - Prints out lists of creatures, classes, etc..."
        "\n\tmaxtime - Sets or displays the max time for event messages to be displayed."
        "\n\ttermwidth - Sets the terminal width."
        "\n\n==Cheats=="
        "\n\taddcreature - Adds a creature."
        "\n\tsetcreaturelevel - Sets the level of a given creature."
        "\n\taddgold - Gives gold to one player."
        "\n\taddmana - Gives mana to one player."
        "\n\ticanseedeadpeople - Toggles on/off fog of war for every connected player."
        "\n\n==Developer\'s options=="
        "\n\tfps - Sets the maximum framerate cap."
        "\n\tambientlight - Sets the ambient light color."
        "\n\tnearclip - Sets the near clipping distance."
        "\n\tfarclip - Sets the far clipping distance."
        "\n\tcreaturevisdebug - Turns on visual debugging for a given creature."    
        "\n\tseatvisdebug - Turns on visual debugging for a given seat."
        "\n\tsetcreaturedest - Sets the creature destination/"
        "\n\tlistmeshanims - Lists all the animations for the given mesh."
        "\n\ttriggercompositor - Starts the given Ogre Compositor."
        "\n\tcatmullspline - Triggers the catmullspline camera movement type."
        "\n\tcirclearound - Triggers the circle camera movement type."
        "\n\tsetcamerafovy - Sets the camera vertical field of view aspect ratio value."
        "\n\tlogfloodfill - Displays the FloodFillValues of all the Tiles in the GameMap.";

//! \brief Template function to get/set a variable from the ODFrameListener object
template<typename ValType, typename Getter, typename Setter>
Command::Result cSetFrameListenerVar(Getter getter,
                        Setter setter,
                        ODFrameListener& fl,
                        const std::string& name,
                        const Command::ArgumentList_t& args, ConsoleInterface& c)
{
    if(args.size() < 2)
    {
        c.print("Value " + name + " is: " + Helper::toString(getter(fl)));
    }
    else
    {
        ValType v = Helper::stringToT<ValType>(args[1]);
        setter(fl, v);
        c.print("Value " + name + " set to " + Helper::toString(v));
    }
    return Command::Result::SUCCESS;
}

Command::Result cSendCmdToServer(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{
    // If we should send a console command, we do it
    if(!ODClient::getSingleton().isConnected())
    {
        c.print("Trying to send a console command while not connected");
        return Command::Result::FAILED;
    }

    if(args.empty())
    {
        c.print("Trying to send an empty console command");
        return Command::Result::INVALID_ARGUMENT;
    }

    ClientNotification *clientNotification = new ClientNotification(
        ClientNotificationType::askExecuteConsoleCommand);
    uint32_t nbArgs = args.size();
    clientNotification->mPacket << nbArgs;
    for(const std::string& str : args)
        clientNotification->mPacket << str;

    ODClient::getSingleton().queueClientNotification(clientNotification);
    return Command::Result::SUCCESS;
}

Command::Result cAmbientLight(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{
    Ogre::SceneManager* mSceneMgr = RenderManager::getSingletonPtr()->getSceneManager();

    if (args.size() < 2)
    {
        // Display the current ambient light values.
        Ogre::ColourValue curLight = mSceneMgr->getAmbientLight();
        c.print("Current ambient light is:" + Helper::toString(curLight));
        return Command::Result::SUCCESS;
    }
    else if(args.size() >= 4)
    {
        // Set the new color.
        Ogre::ColourValue v(Helper::toFloat(args[1]), Helper::toFloat(args[2]), Helper::toFloat(args[3]));

        mSceneMgr->setAmbientLight(v);
        c.print("\nAmbient light set to:\n" +
                Helper::toString(v));
    }
    else
    {
        c.print("To set a colour value, 3 colour values are needed");
        return Command::Result::INVALID_ARGUMENT;
    }
    return Command::Result::SUCCESS;
}

Command::Result cShadowFarClipDistance(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{
    if (args.size() < 2)
    {
        // Display the current shadow far clip distance
        c.print("Current shadow far clip distance is" + Helper::toString(RenderManager::getSingletonPtr()->mHandLight->getShadowFarClipDistance()));
    }
    else if(args.size() >= 2)
    {
        RenderManager::getSingletonPtr()->mHandLight->setShadowFarClipDistance(Helper::toFloat(args[1]));
        c.print("Shadow far clip distance is equal to " + Helper::toString(RenderManager::getSingletonPtr()->mHandLight->getShadowFarClipDistance()));
  
    }
        return Command::Result::SUCCESS;
}

Command::Result cShadowNearClipDistance(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{
    if (args.size() < 2)
    {
        // Display the current shadow near clip distance
        c.print("Current shadow near clip distance is" + Helper::toString(RenderManager::getSingletonPtr()->mHandLight->getShadowNearClipDistance()));
        return Command::Result::SUCCESS;
    }
    else if(args.size() >= 2)
    {
        RenderManager::getSingletonPtr()->mHandLight->setShadowNearClipDistance(Helper::toFloat(args[1]));
        c.print("Shadow near clip distance is equal to " + Helper::toString(RenderManager::getSingletonPtr()->mHandLight->getShadowNearClipDistance()));

    }
        return Command::Result::SUCCESS;  
}


Command::Result cSetOptimalAdjustFactor(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{
    if (args.size() < 2)
    {
        // Display the current shadow near clip distance
        c.print("Current optimal adjust factor parameter n is " + Helper::toString(dynamic_cast<Ogre::LiSPSMShadowCameraSetup*>(RenderManager::getSingletonPtr()->getSceneManager()->getShadowCameraSetup().get())->getOptimalAdjustFactor() ));
        return Command::Result::SUCCESS;
    }
    else if(args.size() >= 2)
    {
        dynamic_cast<Ogre::LiSPSMShadowCameraSetup*>(RenderManager::getSingletonPtr()->getSceneManager()->getShadowCameraSetup().get())->setOptimalAdjustFactor(Helper::toFloat(args[1]));

        
 
    }
        return Command::Result::SUCCESS; 
}

Command::Result cSetUseSimpleOptimalAdjust(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{
    if (args.size() < 2)
    {
        c.print("Current usage of simple optimal adjust is " + Helper::toString(dynamic_cast<Ogre::LiSPSMShadowCameraSetup*>(RenderManager::getSingletonPtr()->getSceneManager()->getShadowCameraSetup().get())->getUseSimpleOptimalAdjust() )) ;
        return Command::Result::SUCCESS;        
    }
    else
    {
        dynamic_cast<Ogre::LiSPSMShadowCameraSetup*>(RenderManager::getSingletonPtr()->getSceneManager()->getShadowCameraSetup().get())->setUseSimpleOptimalAdjust(Helper::toBool(args[1]));
        return Command::Result::SUCCESS; 
    }

}

Command::Result cSetCameraLightDirectionThreshold(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{

    if (args.size() < 2)
    {
        c.print("Current camera light direction threshold is " + Helper::toString(dynamic_cast<Ogre::LiSPSMShadowCameraSetup*>(RenderManager::getSingletonPtr()->getSceneManager()->getShadowCameraSetup().get())->getCameraLightDirectionThreshold().valueDegrees())) ;
        return Command::Result::SUCCESS;        
    }
    else
    {
        dynamic_cast<Ogre::LiSPSMShadowCameraSetup*>(RenderManager::getSingletonPtr()->getSceneManager()->getShadowCameraSetup().get())->setCameraLightDirectionThreshold(Ogre::Degree(Helper::toInt(args[1])));
        return Command::Result::SUCCESS; 
    }

}


Command::Result cGetShadowTextureCount(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{

        c.print("Current shadow texture's number is  " + Helper::toString(RenderManager::getSingletonPtr()->getSceneManager()->getShadowTextureCount())) ;
        return Command::Result::SUCCESS;        
}

Command::Result cSetShadowCameraFovY(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{
        Ogre::Camera* mShadowCam = RenderManager::getSingletonPtr()->getSceneManager()->createCamera("mShadowCam");
        RenderManager::getSingletonPtr()->mHandLight->setSpotlightRange( Ogre::Degree(145),Ogre::Degree(145));       
        RenderManager::getSingletonPtr()->getSceneManager()->getShadowCameraSetup()->getShadowCamera(RenderManager::getSingletonPtr()->getSceneManager(), RenderManager::getSingletonPtr()->getViewport()->getCamera(), RenderManager::getSingletonPtr()->getViewport(), RenderManager::getSingletonPtr()->mHandLight, mShadowCam, 0);
        
        // mShadowCam->setFOVy(Ogre::Degree(90)); 


        
    if (args.size() < 2)
    {

        c.print("Current shadow light camera FOVy is " + Helper::toString(mShadowCam->getFOVy().valueDegrees())) ;
        return Command::Result::SUCCESS;        
    }
    else
    {
        Ogre::Radian rr;
        rr = Ogre::Degree(Helper::toFloat(args[1]));
        return Command::Result::SUCCESS; 
    }

}

Command::Result cFPS(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{
    if(args.size() < 2)
    {
        c.print("\nCurrent maximum framerate is "
                + Helper::toString(ODFrameListener::getSingleton().getMaxFPS())
                + "\n");
    }
    else if(args.size() >= 2)
    {
        int fps = Helper::toInt(args[1]);
        ODFrameListener::getSingleton().setMaxFPS(fps);
        c.print("\nMaximum framerate set to: " + Helper::toString(fps));
    }
    return Command::Result::SUCCESS;
}


Command::Result cGetPosition(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager& am)
{
    InputManager& inputManager = InputManager::getSingleton();
    c.print("X: " + Helper::toString(inputManager.mXPos)+ " Y: " + Helper::toString(inputManager.mYPos));
    return Command::Result::SUCCESS;
}


Command::Result cPrintNodes(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{
    std::function<void(Ogre::SceneNode*)> printNodesAux = [&](Ogre::SceneNode* sn){
        c.print(sn->getName());
        Ogre::Node::ChildNodeMap   chnm = sn->getChildren();
        for(auto it : chnm   ){
            printNodesAux( static_cast<Ogre::SceneNode*>(it));
        } 
    };
    Ogre::SceneManager* mSceneMgr = RenderManager::getSingletonPtr()->getSceneManager();   
    printNodesAux(mSceneMgr->getRootSceneNode());
    return Command::Result::SUCCESS;
}


Command::Result cPrintEntities(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{
    std::function<void(Ogre::SceneNode*)> printNodesAux = [&](Ogre::SceneNode* sn){
  	for(Ogre::MovableObject* mv: sn->getAttachedObjectIterator())
            c.print(mv->getName());
        
        Ogre::Node::ChildNodeMap   chnm = sn->getChildren();
        for(auto it : chnm   ){
            printNodesAux( static_cast<Ogre::SceneNode*>(it));
        } 
    };
    Ogre::SceneManager* mSceneMgr = RenderManager::getSingletonPtr()->getSceneManager();   
    printNodesAux(mSceneMgr->getRootSceneNode());
    return Command::Result::SUCCESS;
}


Command::Result cSrvAddCreature(const Command::ArgumentList_t& args, ConsoleInterface& c, GameMap& gameMap)
{
    if (args.size() < 6)
    {
        c.print("Invalid number of arguments\n");
        return Command::Result::INVALID_ARGUMENT;
    }

    // We remove the first argument (the console command)
    Command::ArgumentList_t tmp = args;
    tmp.erase(tmp.begin());
    std::string str = boost::algorithm::join(tmp, "\t");
    std::stringstream ss(str);
    Creature* creature = Creature::getCreatureFromStream(&gameMap, ss);
    if(creature == nullptr)
    {
        OD_LOG_INF("Cannot creature proper creature from string=" + str);
        return Command::Result::FAILED;
    }

    creature->addToGameMap();
    //Set up definition for creature. This was previously done in createMesh for some reason.
    creature->setupDefinition(gameMap, *ConfigManager::getSingleton().getCreatureDefinitionDefaultWorker());
    //Set position to update info on what tile the creature is in.
    creature->setPosition(creature->getPosition());

    return Command::Result::SUCCESS;
}

Command::Result cList(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager& mm)
{
    ODFrameListener* frameListener = ODFrameListener::getSingletonPtr();
    GameMap* gameMap = frameListener->getClientGameMap();

    if (args.size() < 2)
    {
        c.print("lists available:\n\t\tclasses\tcreatures\tplayers\n\t\tnetwork\trooms\tcolors\n\t\tgoals\n");
        return Command::Result::SUCCESS;
    }

    std::stringstream stringStr;

    if (args[1].compare("creatures") == 0)
    {
        stringStr << "Class:\tCreature name:\tLocation:\tColor:\tLHand:\tRHand\n\n";
        for (Creature* creature : gameMap->getCreatures())
        {
            GameEntity::exportToStream(creature, stringStr);
            stringStr << std::endl;
        }
    }
    else if (args[1].compare("classes") == 0)
    {
        stringStr << "Class:\tMesh:\tHP:\tMana:\tSightRadius:\tDigRate:\tMovespeed:\n\n";
        for (unsigned int i = 0; i < gameMap->numClassDescriptions(); ++i)
        {
            const CreatureDefinition* currentClassDesc = gameMap->getClassDescription(i);
            stringStr << currentClassDesc << "\n";
        }
    }
    else if (args[1].compare("players") == 0)
    {
        stringStr << "Local player:\tNick:\tSeatId\tTeamId:\n\n"
                  << "me\t\t" << gameMap->getLocalPlayer()->getNick() << "\t"
                  << gameMap->getLocalPlayer()->getSeat()->getId() << "\t"
                  << gameMap->getLocalPlayer()->getSeat()->getTeamId() << "\n\n";

        stringStr << "Player:\tNick:\tSeatId\tTeamId:\n\n";

        for (Player* player : gameMap->getPlayers())
        {
            stringStr << player->getId() << "\t\t" << player->getNick() << "\t"
                    << player->getSeat()->getId() << "\t"
                    << player->getSeat()->getTeamId() << "\n\n";
        }
    }
    else if (args[1].compare("network") == 0)
    {
        if (mm.getCurrentModeType() == AbstractModeManager::EDITOR)
        {
            stringStr << "You are currently in the map editor.";
        }
        else if (ODServer::getSingleton().isConnected())
        {
            stringStr << "You are currently acting as a server.";
        }
        else
        {
            stringStr << "You are currently connected to a server.";
        }
    }
    else if (args[1].compare("rooms") == 0)
    {
        stringStr << "Name:\tSeat Id:\tNum tiles:\n\n";
        for (Room* room : gameMap->getRooms())
        {
            stringStr << room->getName() << "\t" << room->getSeat()->getId()
                    << "\t" << room->numCoveredTiles() << "\n";
        }
    }
    else if (args[1].compare("colors") == 0 || args[1].compare("colours") == 0)
    {
        stringStr << "Number:\tRed:\tGreen:\tBlue:\n";
        const std::vector<Seat*> seats = gameMap->getSeats();
        for (Seat* seat : seats)
        {
            Ogre::ColourValue color = seat->getColorValue();

            stringStr << "\n" << seat->getId() << "\t\t" << color.r
                    << "\t\t" << color.g << "\t\t" << color.b;
        }
    }
    else if (args[1].compare("goals") == 0)
    {
        // Loop over the list of unmet goals for the seat we are sitting in an print them.
        stringStr << "Unfinished Goals:\nGoal Name:\tDescription\n----------\t-----------\n";
        for (unsigned int i = 0; i < gameMap->getLocalPlayer()->getSeat()->numUncompleteGoals(); ++i)
        {
            Seat* s = gameMap->getLocalPlayer()->getSeat();
            Goal* tempGoal = s->getUncompleteGoal(i);
            stringStr << tempGoal->getName() << ":\t"
                    << tempGoal->getDescription(*s) << "\n";
        }

        // Loop over the list of completed goals for the seat we are sitting in an print them.
        stringStr << "\n\nCompleted Goals:\nGoal Name:\tDescription\n----------\t-----------\n";
        for (unsigned int i = 0; i < gameMap->getLocalPlayer()->getSeat()->numCompletedGoals(); ++i)
        {
            Seat* seat = gameMap->getLocalPlayer()->getSeat();
            Goal* tempGoal = seat->getCompletedGoal(i);
            stringStr << tempGoal->getName() << ":\t"
                    << tempGoal->getSuccessMessage(*seat) << "\n";
        }
    }
    else
    {
        stringStr << "ERROR:  Unrecognized list.  Type \"list\" with no arguments to see available lists.";
    }

    c.print("+\n" + stringStr.str() + "\n");
    return Command::Result::SUCCESS;
}

Command::Result cSrvCreatureVisDebug(const Command::ArgumentList_t& args, ConsoleInterface& c, GameMap& gameMap)
{
    if(args.size() < 2)
        return Command::Result::INVALID_ARGUMENT;

    const std::string& name = args[1];
    gameMap.consoleToggleCreatureVisualDebug(name);

    return Command::Result::SUCCESS;
}

Command::Result cSrvSeatVisDebug(const Command::ArgumentList_t& args, ConsoleInterface& c, GameMap& gameMap)
{
    if(args.size() < 2)
        return Command::Result::INVALID_ARGUMENT;

    int seatId = Helper::toInt(args[1]);
    gameMap.consoleToggleSeatVisualDebug(seatId);
    return Command::Result::SUCCESS;
}

Command::Result cSrvToggleFOW(const Command::ArgumentList_t& args, ConsoleInterface& c, GameMap& gameMap)
{
    gameMap.consoleAskToggleFOW();
    return Command::Result::SUCCESS;
}

Command::Result cSrvSetCreatureLevel(const Command::ArgumentList_t& args, ConsoleInterface& c, GameMap& gameMap)
{
    if(args.size() < 3)
        return Command::Result::INVALID_ARGUMENT;

    const std::string& name = args[1];
    uint32_t level = Helper::toUInt32(args[2]);
    gameMap.consoleSetLevelCreature(name, level);
    return Command::Result::SUCCESS;
}

Command::Result cCircleAround(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{
    if(args.size() < 4)
    {
        c.print("\nERROR:  You need to specify an circle center "
                          "(two coordinates) and circle radius\n");
        return Command::Result::INVALID_ARGUMENT;
    }

    ODFrameListener& frameListener = ODFrameListener::getSingleton();
    CameraManager* cm = frameListener.getCameraManager();
    int centerX = Helper::toDouble(args[1]);
    int centerY = Helper::toDouble(args[2]);
    unsigned int radius = Helper::toDouble(args[3]);


    cm->circleAround(centerX, centerY, radius);
    return Command::Result::SUCCESS;
}

Command::Result cHermiteCatmullSpline(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{
    //TODO: only works on larger maps with coordinates > 10
    if(args.size() < 5)
    {
        c.print("\nERROR:  You need to specify at least two coordinate's pair.\n");
        return Command::Result::INVALID_ARGUMENT;
    }


    CameraManager* cm = ODFrameListener::getSingleton().getCameraManager();
    std::size_t numPairs = (args.size() - 1) / 2;

    cm->resetHCSNodes(numPairs);
    for(std::size_t i = 1; i < args.size() - 1; i +=2)
    {
        int a = Helper::toInt(args[i]);
        int b = Helper::toInt(args[i + 1]);
        //TODO: Why are the points specified as integers?
        cm->addHCSNodes(a, b);
        c.print("Adding nodes: " + args[i] + ", " + args[i + 1]);
    }

    cm->setCatmullSplineMode(true);
    return Command::Result::SUCCESS;
}

Command::Result cSrvAddGold(const Command::ArgumentList_t& args, ConsoleInterface& c, GameMap& gameMap)
{
    if(args.size() < 3)
        return Command::Result::INVALID_ARGUMENT;

    int seatId = Helper::toInt(args[1]);
    int gold = Helper::toInt(args[2]);
    gameMap.addGoldToSeat(gold, seatId);
    return Command::Result::SUCCESS;
}

Command::Result cSrvAddMana(const Command::ArgumentList_t& args, ConsoleInterface& c, GameMap& gameMap)
{
    if(args.size() < 3)
        return Command::Result::INVALID_ARGUMENT;

    int seatId = Helper::toInt(args[1]);
    int mana = Helper::toInt(args[2]);
    gameMap.addManaToSeat(mana, seatId);
    return Command::Result::SUCCESS;
}

Command::Result cSrvSetCreatureDest(const Command::ArgumentList_t& args, ConsoleInterface& c, GameMap& gameMap)
{
    if(args.size() < 4)
        return Command::Result::INVALID_ARGUMENT;

    const std::string& name = args[1];
    int x = Helper::toInt(args[2]);
    int y = Helper::toInt(args[3]);
    gameMap.consoleSetCreatureDestination(name, x, y);
    return Command::Result::SUCCESS;
}

Command::Result cSrvLogFloodFill(const Command::ArgumentList_t&, ConsoleInterface& c, GameMap& gameMap)
{
    gameMap.logFloodFileTiles();
    return Command::Result::SUCCESS;
}

Command::Result cSetCameraFOVy(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{
    Ogre::Camera* cam = ODFrameListener::getSingleton().getCameraManager()->getActiveCamera();
    if(args.size() < 2)
    {
        c.print("Camera FOVy :" + Helper::toString(cam->getFOVy().valueDegrees()));
    }
    else
    {
        cam->setFOVy(Ogre::Degree(static_cast<Ogre::Real>(Helper::toFloat(args[1]))));
    }
    return Command::Result::SUCCESS;
}

Command::Result cEnableZPrePass(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{
    ODFrameListener::getSingleton().mRenderManager->setup();
    ODFrameListener::getSingleton().mRenderManager->m_ZPrePassEnabled = true;    
    c.print("\nsetting up mRenderManager setup() \n ZPreePass enabled, to see it press CapsLock");
    return Command::Result::SUCCESS;
}

Command::Result cListMeshAnims(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{
    if(args.size() < 2)
    {
        c.print("\nERROR : Need to specify name of mesh");
        return Command::Result::INVALID_ARGUMENT;
    }
    std::string anims = RenderManager::consoleListAnimationsForMesh(args[1]);
    c.print("\nAnimations for " + args[1] + ": " + anims);
    return Command::Result::SUCCESS;
}

Command::Result cSetLogLevel(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{
    if(args.size() < 2)
    {
        c.print("\nERROR : Need to specify log level or module + log level");
        return Command::Result::INVALID_ARGUMENT;
    }

    if(args.size() == 2)
    {
        uint32_t val = Helper::toUInt32(args[1]);
        LogMessageLevel lml = static_cast<LogMessageLevel>(val);
        c.print("\nSetting global log level to " + std::string(LogMessageLevelToString(lml)));
        LogManager::getSingleton().setLevel(lml);
        return Command::Result::SUCCESS;
    }

    const std::string& module = args[1];
    uint32_t val = Helper::toUInt32(args[2]);
    LogMessageLevel lml = static_cast<LogMessageLevel>(val);
    c.print("\nSetting log for module '" + module + "' to " + LogMessageLevelToString(lml));
    LogManager::getSingleton().setModuleLevel(module.c_str(), lml);
    return Command::Result::SUCCESS;
}

Command::Result cSrvUnlockSkills(const Command::ArgumentList_t& args, ConsoleInterface& c, GameMap& gameMap)
{
    gameMap.consoleAskUnlockSkills();
    return Command::Result::SUCCESS;
}

Command::Result cKeys(const Command::ArgumentList_t&, ConsoleInterface& c, AbstractModeManager&)
{
    c.print("|| Action               || US Keyboard layout ||     Mouse      ||\n\
             ==================================================================\n\
             || Pan Left             || Left   -   A       || -              ||\n\
             || Pan Right            || Right  -   D       || -              ||\n\
             || Pan Forward          || Up     -   W       || -              ||\n\
             || Pan Backward         || Down   -   S       || -              ||\n\
             || Rotate Left          || Q                  || -              ||\n\
             || Rotate right         || E                  || -              ||\n\
             || Zoom In              || End                || Wheel Up       ||\n\
             || Zoom Out             || Home               || Wheel Down     ||\n\
             || Tilt Up              || Page Up            || -              ||\n\
             || Tilt Down            || End                || -              ||\n\
             || Change view mode     || V                  || -              ||\n\
             || Select Tile/Creature || -                  || Left Click     ||\n\
             || Drop Creature/Gold   || -                  || Right Click    ||\n\
             || Toggle Debug Info    || F11                || -              ||\n\
             || Toggle Console       || F12                || -              ||\n\
             || Quit Game            || ESC                || -              ||\n\
             || Take screenshot      || Printscreen        || -              ||\n");
    return Command::Result::SUCCESS;
}

Command::Result cTriggerCompositor(const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&)
{
    if(args.size() < 2)
    {
        c.print("ERROR: Needs name of compositor");
        return Command::Result::INVALID_ARGUMENT;
    }
    RenderManager::getSingleton().triggerCompositor(args[1]);
    return Command::Result::SUCCESS;
}

} // namespace <none>


namespace ConsoleCommands
{
    void addConsoleCommands(ConsoleInterface& cl)
    {
        cl.addCommand("addcreature",
                         cSendCmdToServer,
                         cSrvAddCreature,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("addgold",
                         cSendCmdToServer,
                         cSrvAddGold,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("addmana",
                         cSendCmdToServer,
                         cSrvAddMana,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("ambientlight",
                         cAmbientLight,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("catmullspline",
                         cHermiteCatmullSpline,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR},
                         {"hermitecatmullspline" });
 
        cl.addCommand("circlearound",
                         cCircleAround,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("creaturevisdebug",
                         cSendCmdToServer,
                         cSrvCreatureVisDebug,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR},
                         {"creaturevisdebug" });
 
        cl.addCommand("enableZPrePass",
                         cEnableZPrePass,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
    
        cl.addCommand("farclip",
                         [](const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&) {
                             return cSetFrameListenerVar<float>(std::mem_fn(&ODFrameListener::getActiveCameraFarClipDistance),
                                                                std::mem_fn(&ODFrameListener::setActiveCameraFarClipDistance),
                                                                ODFrameListener::getSingleton(),
                                                                "far clip distance", args, c);
                         },
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("fps",
                         cFPS,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("getposition",
                         cGetPosition,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR});    
        cl.addCommand("getshadowtexturecount",
                         cGetShadowTextureCount,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR});    
        cl.addCommand("helpmessage",
                         [](const Command::ArgumentList_t&, ConsoleInterface& c, AbstractModeManager&) {
                             c.print(HELPMESSAGE);
                             return Command::Result::SUCCESS;
                         },
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("keys",
                         cKeys,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR},
                         { });
 
        cl.addCommand("list",
                         cList,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR},
                         {"ls" });
 
        cl.addCommand("listmeshanims",
                         cListMeshAnims,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR},
                         {"listmeshanimations" });
 
        cl.addCommand("logfloodfill",
                         cSendCmdToServer,
                         cSrvLogFloodFill,
                         {AbstractModeManager::ModeType::GAME},
                         { });
 
        cl.addCommand("maxtime",
                         [](const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&) {
                             return cSetFrameListenerVar<float>(std::mem_fn(&ODFrameListener::getEventMaxTimeDisplay),
                                                                std::mem_fn(&ODFrameListener::setEventMaxTimeDisplay),
                                                                ODFrameListener::getSingleton(),
                                                                "event max time display", args, c);
                         },
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("nearclip",
                         [](const Command::ArgumentList_t& args, ConsoleInterface& c, AbstractModeManager&) {
                             return cSetFrameListenerVar<float>(std::mem_fn(&ODFrameListener::getActiveCameraNearClipDistance),
                                                                std::mem_fn(&ODFrameListener::setActiveCameraNearClipDistance),
                                                                ODFrameListener::getSingleton(),
                                                                "near clip distance", args, c);
                         },
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("printentities",
                         cPrintEntities,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR});    
        cl.addCommand("printnodes",
                         cPrintNodes,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("seatvisualdebug",
                         cSendCmdToServer,
                         cSrvSeatVisDebug,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR},
                         {"seatvisdebug" });
 
        cl.addCommand("setShadowFarClipDistance",
                         cShadowFarClipDistance,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("setShadowNearClipDistance",
                         cShadowNearClipDistance,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("setcamerafovy",
                         cSetCameraFOVy,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("setcameralightdirectionthreshold",
                         cSetCameraLightDirectionThreshold,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("setcreaturedest",
                         cSendCmdToServer,
                         cSrvSetCreatureDest,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR},
                         {"setcreaturedestination" });
 
        cl.addCommand("setcreaturelevel",
                         cSendCmdToServer,
                         cSrvSetCreatureLevel,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("setloglevel",
                         cSetLogLevel,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR},
                         { });
 
        cl.addCommand("setoptimaladjustfactor",
                         cSetOptimalAdjustFactor,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("setshadowcamerafovy",
                         cSetShadowCameraFovY,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR});    
        cl.addCommand("setuseoptimaladjust",
                         cSetUseSimpleOptimalAdjust,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("tilevisualdebug",
                         [](const Command::ArgumentList_t&, ConsoleInterface& c, AbstractModeManager&) {
                             GameMode* gm = static_cast<GameMode*>(ODFrameListener::getSingletonPtr()->getModeManager()->getCurrentMode());
                             gm->toggleAllowTileDebugWindow();
                             return Command::Result::SUCCESS;
                         },
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME });
 
    
        cl.addCommand("togglefow",
                         cSendCmdToServer,
                         cSrvToggleFOW,
                         {AbstractModeManager::ModeType::GAME},
                         {"icanseedeadpeople" });
 
        cl.addCommand("triggercompositor",
                         cTriggerCompositor,
                         Command::cStubServer,
                         {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR });
 
        cl.addCommand("unlockskills",
                         cSendCmdToServer,
                         cSrvUnlockSkills,
                         {AbstractModeManager::ModeType::GAME });
    }

} // namespace ConsoleCommands
