"""Verify the navigation catalog against the shipped Ogre room meshes."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
catalog = (repo / 'source/gamemap/RoomObjectBounds.h').read_text()
renderer = (repo / 'source/render/RenderManager.cpp').read_text()
scale_block = renderer.split('    if(renderedMovableEntity->getObjectType() == GameEntityType::buildingObject)', 1)[1].split('    Ogre::Entity* ent = nullptr;', 1)[0]
names = set(re.findall(r'\{"([^"]+)"', catalog))
required = set()
for path in (repo / 'source/rooms').glob('*.cpp'):
    required.update(re.findall(r'new (?:BuildingObject|PersistentObject)\(getGameMap\(\), \*this, "([^"]+)"', path.read_text()))
required.update(re.findall(r'^\s*BedMeshName\s+(\S+)', (repo / 'config/creatures.cfg').read_text(), re.M))
bed_dimensions = set()
for creature in (repo / 'config/creatures.cfg').read_text().split('[Creature]')[1:]:
    creature = creature.split('[/Creature]', 1)[0]
    bed = re.search(r'^\s*BedMeshName\s+(\S+)', creature, re.M)
    dimensions = re.search(r'^\s*BedDim\s+(\d+)\s+(\d+)', creature, re.M)
    if bed and dimensions:
        size = (int(dimensions[1]), int(dimensions[2]))
        bed_dimensions.add((bed[1], *size))
required.update(re.findall(r'return "(Goldstack[^"]+)"', (repo / 'source/entities/TreasuryObject.cpp').read_text()))
assert required <= names, f'Missing room furniture: {sorted(required - names)}'
probe = r'''
#include <Ogre.h>
#include "gamemap/RoomObjectBounds.h"
#include <iostream>
int main(int argc,char** argv){try{
 Ogre::Root root("","","bounds.log");root.loadPlugin("RenderSystem_GL3Plus");
 root.setRenderSystem(root.getAvailableRenderers().front());root.initialise(false);
 Ogre::NameValuePairList opts;opts["hidden"]="true";root.createRenderWindow("Bounds",64,64,false,&opts);
 auto& groups=Ogre::ResourceGroupManager::getSingleton();groups.createResourceGroup("Graphics");
 groups.addResourceLocation(std::string(argv[1])+"/models","FileSystem","Graphics",true);groups.initialiseAllResourceGroups();
 int checks=0,failures=0;
 auto* scene=root.createSceneManager();
 auto* node=scene->getRootSceneNode()->createChildSceneNode();
 struct BedSize{const char* name;int width,height;};
 const BedSize beds[]={BED_SIZES};
 struct BuildingObject {Ogre::Vector2 scale=Ogre::Vector2::ZERO;Ogre::Vector2 getFurnitureScale()const{return scale;}} object;
 auto* renderedMovableEntity=&object;
 for(const auto& row:RoomObjectPath::meshBounds){
  const auto mesh=Ogre::MeshManager::getSingleton().load(std::string(row.name)+".mesh","Graphics");
  const auto& b=mesh->getBounds();
  if(std::isfinite(row.maxZ)){
   ++checks;if(b.getMaximum().z>row.maxZ||row.maxZ-b.getMaximum().z>.00002f){++failures;std::cout<<"FAIL "<<row.name<<" low furniture height\n";}
  }
  for(float delta:{row.minX-b.getMinimum().x,row.minY-b.getMinimum().y,row.maxX-b.getMaximum().x,row.maxY-b.getMaximum().y}){
   ++checks;if(std::abs(delta)>.00002f){++failures;std::cout<<"FAIL "<<row.name<<" stale asset bounds\n";}
  }
  const std::string meshName(row.name);
  node->setScale(Ogre::Vector3::UNIT_SCALE);
  RENDER_SCALE
  const auto scale=RoomObjectPath::furnitureScale(row);
  ++checks;
  const bool bed=std::any_of(std::begin(beds),std::end(beds),[&](const BedSize& size){return meshName==size.name;});
  if(node->getScale()!=Ogre::Vector3(scale.x,scale.y,1)||scale.x<=0||scale.y<=0||(!bed&&(scale.x>1||scale.y>1))){
   ++failures;std::cout<<"FAIL "<<row.name<<" renderer footprint scale\n";
  }
  for(float angle:{0.f,30.f,45.f,90.f,180.f,270.f}){
   const Ogre::Quaternion rotation(Ogre::Degree(angle),Ogre::Vector3::UNIT_Z);
   node->setOrientation(rotation);node->setPosition(5,7,.15f);node->_update(true,false);
   Ogre::AxisAlignedBox rendered;
   for(int i=0;i<8;++i)rendered.merge(node->convertLocalToWorldPosition(b.getAllCorners()[i]));
   Ogre::AxisAlignedBox navigation;
   for(float x:{row.minX,row.maxX})for(float y:{row.minY,row.maxY})
    navigation.merge(Ogre::Vector3(5,7,0)+rotation*Ogre::Vector3(x*scale.x,y*scale.y,0));
   ++checks;
   if(std::abs(rendered.getMinimum().x-navigation.getMinimum().x)>.00003f||
      std::abs(rendered.getMinimum().y-navigation.getMinimum().y)>.00003f||
      std::abs(rendered.getMaximum().x-navigation.getMaximum().x)>.00003f||
      std::abs(rendered.getMaximum().y-navigation.getMaximum().y)>.00003f||
      std::abs(rendered.getMinimum().z-b.getMinimum().z-.15f)>.00003f||
      std::abs(rendered.getMaximum().z-b.getMaximum().z-.15f)>.00003f){
    ++failures;std::cout<<"FAIL "<<row.name<<" transformed visual/navigation bounds or height\n";
   }
  }
 }
 for(const auto& size:beds)for(const auto& row:RoomObjectPath::meshBounds)if(std::string(row.name)==size.name){
  const auto mesh=Ogre::MeshManager::getSingleton().load(std::string(row.name)+".mesh","Graphics");
  const auto& bounds=mesh->getBounds();const auto scale=RoomObjectPath::bedPlacement(row,5,7,size.width,size.height,0,"Creature1").scale;
  const std::string meshName=row.name;object.scale={scale.x,scale.y};
  RENDER_SCALE
  ++checks;if(node->getScale()!=Ogre::Vector3(scale.x,scale.y,1)){++failures;std::cout<<"FAIL per-bed renderer scale\n";}
  for(float base:{0.f,90.f})for(int creature=0;creature<24;++creature){
   const int width=base==0?size.width:size.height,height=base==0?size.height:size.width;
   const std::string owner="Creature"+std::to_string(creature);
   const auto placed=RoomObjectPath::bedPlacement(row,5,7,width,height,base,owner);
   const auto restored=RoomObjectPath::bedPlacement(row,5,7,width,height,base,owner);
   node->setScale(placed.scale.x,placed.scale.y,1);node->setPosition(placed.x,placed.y,0);
   node->setOrientation(Ogre::Quaternion(Ogre::Degree(placed.angle),Ogre::Vector3::UNIT_Z));
   node->_update(true,false);Ogre::AxisAlignedBox actual;
   for(int i=0;i<8;++i)actual.merge(node->convertLocalToWorldPosition(bounds.getAllCorners()[i]));
   ++checks;
   if(std::abs(actual.getSize().x-width*.70f)>.00003f||
      std::abs(actual.getSize().y-height*.70f)>.00003f||
      std::abs((4.5f+width-actual.getMaximum().x)-width*.30f)>.00003f||
      std::abs((actual.getMinimum().y-6.5f)-height*.30f)>.00003f){
    ++failures;std::cout<<"FAIL "<<row.name<<" rotated bed must leave 30 percent right and bottom lanes\n";
   }
   ++checks;
   if(std::abs(actual.getMinimum().x-4.5f)>.00003f||
      std::abs(actual.getMaximum().y-(6.5f+height))>.00003f||
      actual.getMaximum().x>=4.5f+width||actual.getMinimum().y<=6.5f||
      std::abs(placed.angle-base)>4.001f||placed.x!=restored.x||placed.y!=restored.y||placed.angle!=restored.angle){
    ++failures;std::cout<<"FAIL "<<row.name<<" stable rotated corner placement\n";
   }
  }
  ++checks;
  if(RoomObjectPath::bedPlacement(row,5,7,size.width,size.height,0,"Creature1").angle==
     RoomObjectPath::bedPlacement(row,5,7,size.width,size.height,0,"Creature2").angle){
   ++failures;std::cout<<"FAIL creature-specific bed angle\n";
  }
 }
 for(const char* name:{"FenceCorner","FenceStraight","PortalObject","DungeonTempleObject","Bookcase","Podium"})
  for(const auto& row:RoomObjectPath::meshBounds)if(std::string(row.name)==name){
   const auto scale=RoomObjectPath::furnitureScale(row);
   ++checks;if(scale.x!=1||scale.y!=1){++failures;std::cout<<"FAIL unchanged object "<<name<<'\n';}
  }
 // These decorations are attached high on a wall, not standing on the floor.
 for(const char* name:{"WeaponShield1.mesh","WeaponShield2.mesh"}){
  const auto mesh=Ogre::MeshManager::getSingleton().load(name,"Graphics");++checks;
  if(mesh->getBounds().getMinimum().z<.8f){++failures;std::cout<<"FAIL wall decoration now occupies ground\n";}
 }
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''.replace('RENDER_SCALE', scale_block)
probe = probe.replace('BED_SIZES', ','.join(f'{{"{name}",{width},{height}}}' for name, width, height in sorted(bed_dimensions)))
with tempfile.TemporaryDirectory(prefix='odp-room-bounds-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', f'/I{repo / "source"}',
                    f'/I{prefix / "include/OGRE"}', 'check.cpp', '/Fecheck.exe', '/link',
                    f'/LIBPATH:{prefix / "lib"}', 'OgreMain.lib'], cwd=work, check=True)
    result = subprocess.run([str(work / 'check.exe'), str(repo)], cwd=work, capture_output=True, text=True)
    print('\n'.join(line for line in result.stdout.splitlines() if 'CHECKS=' in line or 'FAIL ' in line))
    if result.returncode:
        print(result.stderr)
        result.check_returncode()
