/*
 *  Copyright (C) 2011-2016  OpenDungeons Team
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "render/TreasuryGoldBatch.h"

#include "render/TreasuryCreatureRules.h"
#include "utils/Helper.h"

#include <OgreEntity.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreStaticGeometry.h>
#include <OgreVector3.h>

#include <utility>
#include <vector>

void TreasuryGoldBatch::addPile(Ogre::SceneManager* sceneManager, const std::string& entityName,
    const void* roomKey, Ogre::Entity* entity, Ogre::SceneNode* node, bool settling)
{
    if(sceneManager == nullptr || entity == nullptr || node == nullptr)
        return;

    // A pile of the same name was not removed: forget it first
    removePile(entityName);

    mSceneManager = sceneManager;

    Member member;
    member.mKey = roomKey;
    member.mEntity = entity;
    member.mNode = node;
    member.mSettling = settling;
    member.mBatched = false;
    mMembers[entityName] = member;

    std::map<ChunkKey, Chunk>::iterator chunkIt = mChunks.find(member.mKey);
    if(chunkIt == mChunks.end())
    {
        // A new room batch is built at once
        chunkIt = mChunks.insert(std::make_pair(member.mKey, Chunk())).first;
        chunkIt->second.mSinceRebuild = TreasuryCreatureRules::batchRebuildInterval;
    }
    chunkIt->second.mMembers.insert(entityName);
    // A batch that was not touched for a while is rebuilt at once, a busy one when its interval is over
    chunkIt->second.mDirty = true;
}

void TreasuryGoldBatch::pileSettled(const std::string& entityName)
{
    std::map<std::string, Member>::iterator it = mMembers.find(entityName);
    if(it == mMembers.end() || !it->second.mSettling)
        return;

    it->second.mSettling = false;
    mChunks[it->second.mKey].mDirty = true;
}

void TreasuryGoldBatch::hideUntilBatched(const std::string& entityName)
{
    std::map<std::string, Member>::iterator it = mMembers.find(entityName);
    if(it == mMembers.end() || it->second.mSettling)
        return;

    std::map<ChunkKey, Chunk>::iterator chunkIt = mChunks.find(it->second.mKey);
    if(chunkIt != mChunks.end() && chunkIt->second.mGeometry != nullptr)
        it->second.mEntity->setVisible(false);
}

void TreasuryGoldBatch::removePile(const std::string& entityName)
{
    std::map<std::string, Member>::iterator it = mMembers.find(entityName);
    if(it == mMembers.end())
        return;

    std::map<ChunkKey, Chunk>::iterator chunkIt = mChunks.find(it->second.mKey);
    if(chunkIt != mChunks.end())
    {
        chunkIt->second.mMembers.erase(entityName);
        // The pile may still be shown by the batch, which then has to be rebuilt without it
        if(it->second.mBatched)
            chunkIt->second.mDirty = true;
        // A room that never got a batch and has no pile left is dropped
        else if(chunkIt->second.mMembers.empty() && chunkIt->second.mGeometry == nullptr)
            mChunks.erase(chunkIt);
    }
    mMembers.erase(it);
}

void TreasuryGoldBatch::update(float timeSinceLastFrame)
{
    for(std::map<ChunkKey, Chunk>::iterator it = mChunks.begin(); it != mChunks.end();)
    {
        Chunk& chunk = it->second;
        chunk.mSinceRebuild += timeSinceLastFrame;
        if(!chunk.mDirty || chunk.mSinceRebuild < TreasuryCreatureRules::batchRebuildInterval)
        {
            ++it;
            continue;
        }

        rebuild(chunk);
        if(chunk.mMembers.empty())
        {
            if(chunk.mGeometry != nullptr && mSceneManager != nullptr)
                mSceneManager->destroyStaticGeometry(chunk.mGeometry);
            it = mChunks.erase(it);
            continue;
        }
        ++it;
    }
}

void TreasuryGoldBatch::rebuild(Chunk& chunk)
{
    chunk.mDirty = false;
    chunk.mSinceRebuild = 0.0f;

    if(mSceneManager == nullptr)
        return;

    if(chunk.mGeometry == nullptr)
    {
        const std::string name = "TreasuryGoldBatch_" + Helper::toString(++mGeometryNumber);
        chunk.mGeometry = mSceneManager->createStaticGeometry(name);
        // One region per room: the region is larger than any room, so the room is drawn as one batch
        chunk.mGeometry->setOrigin(Ogre::Vector3(-500.0f, -500.0f, -500.0f));
        chunk.mGeometry->setRegionDimensions(Ogre::Vector3(1000.0f, 1000.0f, 1000.0f));
    }

    chunk.mGeometry->reset();

    std::vector<Member*> batched;
    for(std::set<std::string>::iterator it = chunk.mMembers.begin(); it != chunk.mMembers.end(); ++it)
    {
        std::map<std::string, Member>::iterator memberIt = mMembers.find(*it);
        if(memberIt == mMembers.end())
            continue;

        Member& member = memberIt->second;
        // A settling pile is a visible entity of its own, its height changes every frame
        if(member.mSettling)
        {
            member.mBatched = false;
            member.mEntity->setVisible(true);
            continue;
        }

        chunk.mGeometry->addEntity(member.mEntity, member.mNode->getPosition(), member.mNode->getOrientation(),
            member.mNode->getScale());
        batched.push_back(&member);
    }

    // The batch is built before the entities are hidden, so the piles never disappear for a frame
    if(!batched.empty())
        chunk.mGeometry->build();
    for(std::vector<Member*>::iterator it = batched.begin(); it != batched.end(); ++it)
    {
        (*it)->mEntity->setVisible(false);
        (*it)->mBatched = true;
    }
}

void TreasuryGoldBatch::clear()
{
    if(mSceneManager != nullptr)
    {
        for(std::map<ChunkKey, Chunk>::iterator it = mChunks.begin(); it != mChunks.end(); ++it)
        {
            if(it->second.mGeometry != nullptr)
                mSceneManager->destroyStaticGeometry(it->second.mGeometry);
        }
    }
    mChunks.clear();
    mMembers.clear();
}

size_t TreasuryGoldBatch::getBatchCount() const
{
    return mChunks.size();
}
