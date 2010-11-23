#include <iostream>
#include <sstream>

#include "Functions.h"
#include "MissileObject.h"

MissileObject::MissileObject()
{
	initialize();
}

MissileObject::MissileObject(string nMeshName, Ogre::Vector3 nPosition)
{
	initialize();

	meshName = nMeshName;
	sem_wait(&positionLockSemaphore);
	position = nPosition;
	sem_post(&positionLockSemaphore);
}

void MissileObject::initialize()
{
	static int uniqueNumber = 1;
	std::stringstream tempSS;
	tempSS << "Missile_Object_" << uniqueNumber++;
	name = tempSS.str();

	sem_init(&positionLockSemaphore, 0, 1);
	sem_wait(&positionLockSemaphore);
	position = Ogre::Vector3(0,0,0);
	sem_post(&positionLockSemaphore);

	meshesExist = false;
}

/*! \brief Changes the missile's position to a new position.
 *
 *  This is an overloaded function which just calls Creature::setPosition(double x, double y, double z).
 */
void MissileObject::setPosition(Ogre::Vector3 v)
{
	sem_wait(&positionLockSemaphore);
	position = v;
	sem_post(&positionLockSemaphore);

	// Create a RenderRequest to notify the render queue that the scene node for this creature needs to be moved.
	RenderRequest *request = new RenderRequest;
	request->type = RenderRequest::moveSceneNode;
	request->str = name + "_node";
	request->vec = position;

	// Add the request to the queue of rendering operations to be performed before the next frame.
	queueRenderRequest(request);
}

/*! \brief Changes the missile's position to a new position.
 *
 *  Moves the creature to a new location in 3d space.  This function is
 *  responsible for informing OGRE anything it needs to know, as well as
 *  maintaining the list of creatures in the individual tiles.
 */
void MissileObject::setPosition(double x, double y, double z)
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
	if(meshesExist)
		return;

	meshesExist = true;

	RenderRequest *request = new RenderRequest;
	request->type = RenderRequest::createMissileObject;
	request->p = this;

	// Add the request to the queue of rendering operations to be performed before the next frame.
	queueRenderRequest(request);
}

void MissileObject::destroyMesh()
{
	if(!meshesExist)
		return;

	meshesExist = false;

	RenderRequest *request = new RenderRequest;
	request->type = RenderRequest::destroyMissileObject;
	request->p = this;

	// Add the request to the queue of rendering operations to be performed before the next frame.
	queueRenderRequest(request);
}

void MissileObject::deleteYourself()
{
	if(meshesExist)
		destroyMesh();

	// Create a render request asking the render queue to actually do the deletion of this creature.
	RenderRequest *request = new RenderRequest;
	request->type = RenderRequest::deleteMissileObject;
	request->p = this;

	// Add the requests to the queue of rendering operations to be performed before the next frame.
	queueRenderRequest(request);
}
