"""Compile the actual preview renderer against a geometry-recording scene stub."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / 'source/render/RenderManager.cpp').read_text()
method = source[source.index('void RenderManager::rrDrawTilePreview('):
                source.index('\nvoid RenderManager::entitySlapped(')]
code = r'''
#include <vector>
#include <string>
#include <cmath>
#include <iostream>
namespace Ogre {
struct Vector3 {float x,y,z;Vector3(float a,float b,float c):x(a),y(b),z(c){}};
struct ColourValue {float r,g,b;};
struct Bounds {Vector3 getMaximum(){return {0,0,2};}};
struct MovableObject {Bounds getWorldBoundingBox(bool){return {};}};
struct RenderOperation {enum Type {OT_LINE_LIST,OT_TRIANGLE_LIST};};
struct ManualObject {
 struct Section {RenderOperation::Type type;std::vector<Vector3> points;std::vector<ColourValue> colours;};
 std::vector<Section> sections;
 void setDynamic(bool){} void setCastShadows(bool){} void clear(){sections.clear();}
 void begin(const char*,RenderOperation::Type type,const char*){sections.push_back({type,{},{}});}
 void position(Vector3 v){sections.back().points.push_back(v);} void colour(ColourValue c){sections.back().colours.push_back(c);}
 void end(){if(sections.back().points.empty())sections.pop_back();}
};
struct SceneNode {SceneNode* createChildSceneNode(const char*){return this;}void attachObject(ManualObject*){}};
struct SceneManager {ManualObject object;SceneNode node;MovableObject wall;
 ManualObject* createManualObject(const char*){return &object;}SceneNode* getRootSceneNode(){return &node;}
 bool hasEntity(const std::string&){return true;}MovableObject* getEntity(const std::string&){return &wall;}
};
}
struct Tile {int x,y;bool full;int getX(){return x;}int getY(){return y;}bool isFullTile(){return full;}
 Ogre::MovableObject* getFogOfWarMesh(){return nullptr;}std::string getOgreNamePrefix(){return "tile";}std::string getName(){return "1";}};
struct RenderManager {Ogre::SceneManager* mSceneManager;Ogre::ManualObject* mTilePreview=nullptr;
 void rrDrawTilePreview(const std::vector<Tile*>&,const Ogre::ColourValue&,bool=false,bool=false);};
METHOD
int main(){int checks=0,failures=0;auto check=[&](bool pass,const char* label){++checks;if(!pass){++failures;std::cout<<"FAIL "<<label<<'\n';}};
 Ogre::SceneManager scene;RenderManager render{&scene};Tile floor{3,4,false},wall{5,6,true},other{4,4,false};
 render.rrDrawTilePreview({}, {1,1,1}, true);check(!render.mTilePreview,"empty selection allocates nothing");
 for(auto colour:{Ogre::ColourValue{.35f,.3f,1},Ogre::ColourValue{1,.15f,.1f}}){
  render.rrDrawTilePreview({&floor,&wall,&other},colour,true);
  auto& sections=scene.object.sections;
  check(sections.size()==2,"lines and construction ribbon are separate");
  check(sections[0].points.size()==40,"floor edges and wall box preserved");
  auto& ribbon=sections[1];check(ribbon.type==Ogre::RenderOperation::OT_TRIANGLE_LIST,"ribbon uses filled triangles");
  check(ribbon.points.size()==48,"eight triangles per floor, none on wall");
  float area=0;
  for(size_t i=0;i<ribbon.points.size();i+=3){auto a=ribbon.points[i],b=ribbon.points[i+1],c=ribbon.points[i+2];
   float cross=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);check(cross>0,"ribbon faces upward");area+=cross*.5f;}
  check(std::abs(area-2*(1-.88f*.88f))<.0001f,"six-percent border width and open centre");
  for(size_t i=0;i<ribbon.points.size();++i){auto p=ribbon.points[i];auto c=ribbon.colours[i];float x=i<24?3:4;
   check(std::abs(p.x-x)<=.50001f&&std::abs(p.y-4)<=.50001f,"ribbon stays inside selected tile");
   check(std::abs(p.z-.045f)<.00001f,"ribbon clears floor");
   check(c.r==colour.r&&c.g==colour.g&&c.b==colour.b,"validity colour preserved");}
 }
 render.rrDrawTilePreview({&floor,&wall},{1,1,1});
 check(scene.object.sections.size()==1&&scene.object.sections[0].points.size()==32,"non-construction unchanged");
 render.rrDrawTilePreview({&wall},{.35f,.3f,1},false,true);
 auto& digging=scene.object.sections;
 check(digging.size()==2,"digging adds filled frame bands");
 check(digging[1].points.size()==120,"digging outlines top and all four wall faces");
 for(const auto& p:digging[1].points){
  check(std::abs(p.x-5)<=.5051f&&std::abs(p.y-6)<=.5051f,"dig frame stays on the selected wall surface");
  check(p.z>=.0399f&&p.z<=2.0201f,"dig frame follows actual wall height");
 }
 float topArea=0;
 for(size_t i=0;i<digging[1].points.size();i+=3){const auto& a=digging[1].points[i];const auto& b=digging[1].points[i+1];const auto& c=digging[1].points[i+2];
  if(a.z==2.02f&&b.z==a.z&&c.z==a.z)topArea+=((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))*.5f;}
 check(std::abs(topArea-(1-.88f*.88f))<.0001f,"digging top has the accepted construction border width");
 render.rrDrawTilePreview({&wall,&wall},{.35f,.3f,1},false,true);
 check(scene.object.sections[1].points.size()==240,"drag selection frames every selected wall");
 render.rrDrawTilePreview({}, {1,1,1},true);check(scene.object.sections.empty(),"cancel clears ribbon and lines");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
'''.replace('METHOD', method)
with tempfile.TemporaryDirectory(prefix='odp-construction-preview-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(code)
    subprocess.run(['cl', '/nologo', '/EHsc', '/std:c++14', 'check.cpp', '/Fecheck.exe'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)

game = (repo / 'source/modes/GameMode.cpp').read_text()
update = game.split('void GameMode::updateSelectedTiles()', 1)[1].split('void GameMode::unselectAllTiles()', 1)[0]
assert update.count('rrDrawTilePreview(mSelectedTiles, colour, building, digging)') == 2
assert 'SelectedAction::buildRoom' in update and 'SelectedAction::buildTrap' in update
assert 'SelectedAction::none' in update and 'SelectedAction::selectTile' in update
print('Construction action dispatch checks passed')
