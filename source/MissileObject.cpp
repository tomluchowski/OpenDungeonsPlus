#include <iostream>
#include <sstream>

#include "MissileObject.h"
#include "RenderRequest.h"
#include "RenderManager.h"
#include "GameMap.h"
#include "LogManager.h"

sem_t MissileObject::missileObjectUniqueNumberLockSemaphore;

MissileObject::MissileObject(const std::string& nMeshName, const Ogre::Vector3& nPosition, GameMap& gameMap) :
        gameMap(gameMap)
{
    static int uniqueNumber = 0;
LogManager::getSingleton().logMessage("ONE");
    sem_init(&positionLockSemaphore, 0, 1);

    std::stringstream tempSS;
    sem_wait(&missileObjectUniqueNumberLockSemaphore);
    tempSS << "Missile_Object_" << ++uniqueNumber;
    sem_post(&missileObjectUniqueNumberLockSemaphore);
    setName(tempSS.str());
    LogManager::getSingleton().logMessage("TWO");
    setMeshName(nMeshName);
    setMeshExisting(false);

    sem_wait(&positionLockSemaphore);
    position = nPosition;
    sem_post(&positionLockSemaphore);
    LogManager::getSingleton().logMessage("THREE");
}

bool MissileObject::doUpkeep()
{
	// check if we collide with a creature, if yes, do some damage and delete ourselves
    return true;
}


/*! \brief The missile reach the end of the travel, it's destroyed
 *
 */
void MissileObject::stopWalking()
{
	MovableGameEntity::stopWalking();
	gameMap.removeMissileObject(this);
	deleteYourself();
}

/*! \brief Changes the missile's position to a new position.
 *  Moves the creature to a new location in 3d space.  This function is
 *  responsible for informing OGRE anything it needs to know, as well as
 *  maintaining the list of creatures in the individual tiles.
 */
void MissileObject::setPosition(const Ogre::Vector3& v)
{
    sem_wait(&positionLockSemaphore);
    position = v;
    sem_post(&positionLockSemaphore);

    // Create a RenderRequest to notify the render queue that the scene node for this creature needs to be moved.
    RenderRequest *request = new RenderRequest;
    request->type = RenderRequest::moveSceneNode;
    request->str = getName() + "_node";
    request->vec = position;

    // Add the request to the queue of rendering operations to be performed before the next frame.
    RenderManager::queueRenderRequest(request);
}

/*! \brief Changes the missile's position to a new position.
 *
 *  Moves the creature to a new location in 3d space.  This function is
 *  responsible for informing OGRE anything it needs to know, as well as
 *  maintaining the list of creatures in the individual tiles.
 */
void MissileObject::setPosition(Ogre::Real x, Ogre::Real y, Ogre::Real z)
{
    setPosition(Ogre::Vector3(x, y, z));
}

/*! \brief A simple accessor function to get the creature's current position in 3d space.
 *
 */
Ogre::Vector3 MissileObject::getPosition()
{
    sem_wait(&positionLockSemaphore);
    Ogre::Vector3 tempVector(position);
    sem_post(&positionLockSemaphore);

    return tempVector;
}

void MissileObject::createMesh()
{
    std::cout << "\nCalling createMesh()";
    if (isMeshExisting())
        return;

    setMeshExisting(true);

    RenderRequest *request = new RenderRequest;
    request->type = RenderRequest::createMissileObject;
    request->p = static_cast<void*>(this);

    // Add the request to the queue of rendering operations to be performed before the next frame.
    RenderManager::queueRenderRequest(request);
}

void MissileObject::destroyMesh()
{
    if (!isMeshExisting())
        return;

    setMeshExisting(false);

    RenderRequest *request = new RenderRequest;
    request->type = RenderRequest::destroyMissileObject;
    request->p = static_cast<void*>(this);

    // Add the request to the queue of rendering operations to be performed before the next frame.
    RenderManager::queueRenderRequest(request);
}

void MissileObject::deleteYourself()
{
    if (isMeshExisting())
        destroyMesh();

    // Create a render request asking the render queue to actually do the deletion of this creature.
    RenderRequest *request = new RenderRequest;
    request->type = RenderRequest::deleteMissileObject;
    request->p = static_cast<void*>(this);

    // Add the requests to the queue of rendering operations to be performed before the next frame.
    RenderManager::queueRenderRequest(request);
}

