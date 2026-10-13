"""Check the actual building-object scale packet methods with the real codec."""
from pathlib import Path
import os
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
source = (repo / 'source/entities/BuildingObject.cpp').read_text()
start = source.index('void BuildingObject::exportToPacket(')
end = source.index('\nvoid BuildingObject::fireRefresh()', start)
methods = source[start:end]
probe = r'''
#include "network/ODPacket.h"
#include <OgreVector2.h>
#include <iostream>
#include <stdexcept>
#define OD_ASSERT_TRUE(value) do{if(!(value))throw std::runtime_error("invalid packet");}while(0)
struct Seat {};
struct RenderedMovableEntity {
 void exportToPacket(ODPacket& packet,const Seat*)const{packet<<uint32_t(123);}
 void importFromPacket(ODPacket& packet){uint32_t marker=0;OD_ASSERT_TRUE(packet>>marker);OD_ASSERT_TRUE(marker==123);}
};
struct BuildingObject:RenderedMovableEntity {
 Ogre::Vector2 mFurnitureScale=Ogre::Vector2::ZERO;
 void exportToPacket(ODPacket&,const Seat*)const;
 void importFromPacket(ODPacket&);
};
METHODS
int main(){try{
 int checks=0;
 for(const auto scale:{Ogre::Vector2::ZERO,Ogre::Vector2(.75f,.54f),Ogre::Vector2(1.1f,1.7f)}){
  BuildingObject source,destination;source.mFurnitureScale=scale;ODPacket packet;
  source.exportToPacket(packet,nullptr);packet<<uint32_t(456);
  destination.importFromPacket(packet);++checks;OD_ASSERT_TRUE(destination.mFurnitureScale==scale);
  uint32_t following=0;OD_ASSERT_TRUE(packet>>following);++checks;OD_ASSERT_TRUE(following==456);
 }
 ODPacket truncated;truncated<<uint32_t(123)<<.75f;BuildingObject destination;bool rejected=false;
 try{destination.importFromPacket(truncated);}catch(const std::runtime_error&){rejected=true;}
 ++checks;OD_ASSERT_TRUE(rejected);
 std::cout<<"CHECKS="<<checks<<" FAILURES=0\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
'''.replace('METHODS', methods)
with tempfile.TemporaryDirectory(prefix='odp-building-scale-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14',
                    f'/I{repo / "source"}', f'/I{prefix / "include"}', f'/I{prefix / "include/OGRE"}',
                    'check.cpp', str(repo / 'source/network/ODPacket.cpp'), '/Fecheck.exe', '/link',
                    f'/LIBPATH:{prefix / "lib"}', 'OgreMain.lib', 'sfml-network.lib', 'sfml-system.lib'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
