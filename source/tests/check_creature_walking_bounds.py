"""Check body clearance against 121 skinned Walk poses of every creature model."""
from pathlib import Path
import argparse
import os
import re
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--height-profile', action='store_true', help='Measure triangle-clipped walking envelopes below furniture heights without changing collision policy')
parser.add_argument('--frames', type=int, default=120, help='Number of equal Walk intervals to sample, including both ends')
parser.add_argument('--bone-profile', help='Report dominant-bone vertex extents for one mesh, without changing poses or collision policy')
args = parser.parse_args()
assert args.frames > 0

repo = Path(__file__).resolve().parents[2]
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
models = set(re.findall(r'^\s*MeshName\s+(\S+)', (repo / 'config/creatures.cfg').read_text(), re.M))
catalog = set(re.findall(r'\{"([^"\n]+\.mesh)"', (repo / 'source/gamemap/RoomObjectBounds.h').read_text()))
assert models == catalog, f'Walking bounds catalog mismatch: {models ^ catalog}'
assert args.bone_profile is None or args.bone_profile in catalog
renderer = (repo / 'source/render/RenderManager.cpp').read_text()
bed_support = renderer.split('Ogre::Vector3 getBedSupportPoint(', 1)[1].split('\nbool needsCreatureDropFallback(', 1)[0]
bed_names = sorted(set(re.findall(r'^\s*BedMeshName\s+(\S+)', (repo / 'config/creatures.cfg').read_text(), re.M)))
bed_names = sorted(set(bed_names) | {name for name in re.findall(r'\{"([^"\n]+)"',
    (repo / 'source/gamemap/RoomObjectBounds.h').read_text()) if name.endswith(('Bed', 'Coffin'))})
leg_names = {}
for side in ('left', 'right'):
    for joint in ('Upper', 'Lower', 'Tip'):
        leg_names[(side, joint)] = re.search(side + r'Leg\.m' + joint + r' = findFeedingBone\(skeleton, (\{[^\n]+\})\);', renderer)[1]
probe = r'''
#include <Ogre.h>
#include "gamemap/RoomObjectBounds.h"
#include <iostream>
Ogre::Vector3 getBedSupportPoint(BED_SUPPORT
Ogre::Bone* findBone(Ogre::Skeleton* skeleton,std::initializer_list<const char*> names){
 for(const auto* name:names)if(skeleton->hasBone(name))return skeleton->getBone(name);
 return nullptr;
}
Ogre::AxisAlignedBox poseBounds;
const float heights[]={.05f,.1f,.15f,.2f,.3f,RoomObjectPath::lowWalkingHeight};
Ogre::AxisAlignedBox heightBounds[6];
bool profile=false;
const std::string boneProfile="BONE_PROFILE";
std::map<unsigned short,Ogre::AxisAlignedBox> boneBounds,lowBoneBounds;
float radius(Ogre::Entity* entity){
 entity->addSoftwareAnimationRequest(false);entity->_updateAnimation();entity->removeSoftwareAnimationRequest(false);
 float result=0;poseBounds.setNull();
 for(unsigned sub=0;sub<entity->getNumSubEntities();++sub){
  auto* part=entity->getSubEntity(sub);
  auto* data=part->getSubMesh()->useSharedVertices?entity->_getSkelAnimVertexData():part->_getSkelAnimVertexData();
  auto* position=data->vertexDeclaration->findElementBySemantic(Ogre::VES_POSITION);
  auto buffer=data->vertexBufferBinding->getBuffer(position->getSource());
  Ogre::HardwareBufferLockGuard lock(buffer,Ogre::HardwareBuffer::HBL_READ_ONLY);
  auto* bytes=static_cast<unsigned char*>(lock.pData);
  for(size_t i=0;i<data->vertexCount;++i){
   float* vertex;position->baseVertexPointerToElement(bytes+(data->vertexStart+i)*buffer->getVertexSize(),&vertex);
   result=std::max(result,std::hypot(vertex[0],vertex[1]));poseBounds.merge(Ogre::Vector3(vertex));
   if(entity->getMesh()->getName()==boneProfile){
    const auto& assignments=part->getSubMesh()->useSharedVertices?entity->getMesh()->getBoneAssignments():part->getSubMesh()->getBoneAssignments();
    const auto range=assignments.equal_range(i);auto strongest=range.second;
    for(auto weight=range.first;weight!=range.second;++weight)
     if(strongest==range.second||weight->second.weight>strongest->second.weight)strongest=weight;
    if(strongest==range.second)throw std::runtime_error("Bone profile found an unweighted vertex");
    const auto bone=strongest->second.boneIndex;boneBounds[bone].merge(Ogre::Vector3(vertex));
    if(vertex[2]<=RoomObjectPath::lowWalkingHeight)lowBoneBounds[bone].merge(Ogre::Vector3(vertex));
   }
  }
  {
   if(part->getSubMesh()->operationType!=Ogre::RenderOperation::OT_TRIANGLE_LIST)
    throw std::runtime_error("Height profiling requires triangle-list meshes");
   auto* indices=part->getSubMesh()->indexData;
   Ogre::HardwareBufferLockGuard indexLock(indices->indexBuffer,Ogre::HardwareBuffer::HBL_READ_ONLY);
   const bool wide=indices->indexBuffer->getType()==Ogre::HardwareIndexBuffer::IT_32BIT;
   for(size_t triangle=0;triangle+2<indices->indexCount;triangle+=3){
    Ogre::Vector3 points[3];
    for(int corner=0;corner<3;++corner){
     const auto offset=indices->indexStart+triangle+corner;
     const auto index=wide?static_cast<const uint32_t*>(indexLock.pData)[offset]:static_cast<const uint16_t*>(indexLock.pData)[offset];
     if(index>=data->vertexCount)throw std::runtime_error("Height profile vertex index outside animated data");
     float* vertex;position->baseVertexPointerToElement(bytes+(data->vertexStart+index)*buffer->getVertexSize(),&vertex);points[corner]=Ogre::Vector3(vertex);
    }
    for(int band=0;band<6;++band)for(int corner=0;corner<3;++corner){
     if(!profile&&band!=5)continue;
     const auto& a=points[corner];const auto& b=points[(corner+1)%3];
     if(a.z<=heights[band])heightBounds[band].merge(a);
     if((a.z<heights[band]&&b.z>heights[band])||(a.z>heights[band]&&b.z<heights[band]))
      heightBounds[band].merge(a+(b-a)*((heights[band]-a.z)/(b.z-a.z)));
    }
   }
  }
 }
 return result;
}
int main(int argc,char** argv){try{
 Ogre::Root root("","","walking-bounds.log");root.loadPlugin("RenderSystem_GL3Plus");
 root.setRenderSystem(root.getAvailableRenderers().front());root.initialise(false);
 Ogre::NameValuePairList opts;opts["hidden"]="true";root.createRenderWindow("Walking bounds",64,64,false,&opts);
 auto& groups=Ogre::ResourceGroupManager::getSingleton();groups.createResourceGroup("Graphics");
 groups.addResourceLocation(std::string(argv[1])+"/models","FileSystem","Graphics",true);groups.initialiseAllResourceGroups();
 auto* scene=root.createSceneManager();int checks=0,failures=0;
 profile=argc>3;
 const int frames=std::stoi(argv[2]);
 if(profile)for(const std::string name:{BED_NAMES}){
  const auto mesh=Ogre::MeshManager::getSingleton().load(name+".mesh","Graphics");
  const auto support=getBedSupportPoint(mesh);
  ++checks;if(!std::isfinite(support.z)||support.z<mesh->getBounds().getMinimum().z-.00001f||support.z>mesh->getBounds().getMaximum().z+.00001f){++failures;std::cout<<"FAIL invalid bed support "<<name<<'\n';}
  std::cout<<"FURNITURE_HEIGHT "<<name<<" min="<<mesh->getBounds().getMinimum().z<<" max="<<mesh->getBounds().getMaximum().z<<" support="<<support.z<<'\n';
 }
 for(const auto& model:RoomObjectPath::walkingRadii){
  boneBounds.clear();lowBoneBounds.clear();
  for(auto& bounds:heightBounds)bounds.setNull();
  auto* entity=scene->createEntity(model.name,model.name,"Graphics");
  auto* node=scene->getRootSceneNode()->createChildSceneNode();node->attachObject(entity);
  auto* walk=entity->getAnimationState("Walk");walk->setEnabled(true);
  auto* skeleton=entity->getSkeleton();
  Ogre::Bone* joints[]={LEFT_UPPER,LEFT_LOWER,LEFT_TIP,RIGHT_UPPER,RIGHT_LOWER,RIGHT_TIP};
  Ogre::AxisAlignedBox jointBounds[6];
  for(int frame=0;frame<=frames;++frame){
   walk->setTimePosition(walk->getLength()*frame/frames);root._fireFrameStarted();root._fireFrameRenderingQueued();
   float actual=radius(entity);++checks;
   if(profile)for(int joint=0;joint<6;++joint)if(joints[joint])jointBounds[joint].merge(joints[joint]->_getDerivedPosition());
   if(actual>model.radius||poseBounds.getMinimum().x<model.minX||poseBounds.getMinimum().y<model.minY||poseBounds.getMaximum().x>model.maxX||poseBounds.getMaximum().y>model.maxY){++failures;std::cout<<"FAIL "<<model.name<<" pose "<<frame<<" exceeds walking bounds "<<poseBounds<<'\n';}
   root._fireFrameEnded();
  }
  bool foundLow=false;
  for(const auto& low:RoomObjectPath::lowWalkingBounds)if(std::string(low.name)==model.name){
   foundLow=true;const auto& actual=heightBounds[5];const float margin=RoomObjectPath::lowWalkingMargin;
   ++checks;if(low.empty?!actual.isNull():(actual.isNull()||actual.getMinimum().x<low.minX-margin||
    actual.getMinimum().y<low.minY-margin||actual.getMaximum().x>low.maxX+margin||actual.getMaximum().y>low.maxY+margin||
    actual.getMinimum().z<low.minZ-margin)){
    ++failures;std::cout<<"FAIL low walking envelope "<<model.name<<" "<<actual<<'\n';
   }
  }
  ++checks;if(!foundLow){++failures;std::cout<<"FAIL missing low walking envelope "<<model.name<<'\n';}
  if(profile)for(int band=0;band<6;++band)
   std::cout<<"HEIGHT_PROFILE "<<model.name<<" z="<<heights[band]<<" bounds="<<heightBounds[band]<<'\n';
  if(profile)for(int joint=0;joint<6;++joint){
   std::cout<<"LEG_PROFILE "<<model.name<<" joint="<<joint;
   if(joints[joint])std::cout<<" bone="<<joints[joint]->getName()<<" bounds="<<jointBounds[joint];else std::cout<<" missing";
   std::cout<<'\n';
  }
  if(std::string(model.name)==boneProfile)for(const auto& part:boneBounds){
   std::cout<<"DOMINANT_BONE "<<model.name<<" bone="<<skeleton->getBone(part.first)->getName()<<" bounds="<<part.second;
   const auto low=lowBoneBounds.find(part.first);
   if(low!=lowBoneBounds.end())std::cout<<" low="<<low->second;
   std::cout<<'\n';
  }
  node->detachAllObjects();scene->destroyEntity(entity);scene->destroySceneNode(node);
 }
 root.destroySceneManager(scene);std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''
probe = probe.replace('BED_SUPPORT', bed_support).replace('BED_NAMES', ','.join('"' + name + '"' for name in bed_names))
probe = probe.replace('BONE_PROFILE', args.bone_profile or '')
for side in ('left', 'right'):
    for joint in ('Upper', 'Lower', 'Tip'):
        probe = probe.replace(side.upper() + '_' + joint.upper(), 'findBone(skeleton,' + leg_names[(side, joint)] + ')')
with tempfile.TemporaryDirectory(prefix='odp-walking-bounds-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', f'/I{repo / "source"}',
                    f'/I{prefix / "include/OGRE"}', 'check.cpp', '/Fecheck.exe', '/link',
                    f'/LIBPATH:{prefix / "lib"}', 'OgreMain.lib'], cwd=work, check=True)
    result = subprocess.run([str(work / 'check.exe'), str(repo), str(args.frames)] + (['profile'] if args.height_profile else []), cwd=work, capture_output=True, text=True)
    print('\n'.join(line for line in result.stdout.splitlines() if any(marker in line for marker in ('CHECKS=', 'FAIL ', 'HEIGHT_PROFILE ', 'FURNITURE_HEIGHT ', 'LEG_PROFILE ', 'DOMINANT_BONE '))))
    if result.returncode:
        if 'FAIL ' not in result.stdout:
            print(result.stderr)
        result.check_returncode()
