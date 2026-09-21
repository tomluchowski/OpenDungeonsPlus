"""Check the production worker silhouette, shared image binding and icon style."""
from pathlib import Path
import os
import subprocess
import tempfile
import xml.etree.ElementTree as ET

repo = Path(__file__).resolve().parents[2]
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
(repo / 'build' / 'review-followups').mkdir(parents=True, exist_ok=True)
gui = (repo / 'source/render/Gui.cpp').read_text()
game = (repo / 'source/modes/GameMode.cpp').read_text()


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


refresh = function(game, 'void GameMode::refreshSkillButtonState(')
assert 'getCreatureHandIconImage' not in refresh
assert 'getWorkerClassToSpawn' not in refresh
assert 'mRootWindow->getChild(button)->getProperty("NormalImage")' in game
for filename, prop in [('WindowTabSpells.layout', 'NormalImage'), ('WindowSkillTree.layout', 'ButtonImage')]:
    button = ET.parse(repo / 'gui' / filename).find('.//Window[@name="SummonWorkerButton"]')
    assert button.find(f'Property[@name="{prop}"]').get('value') == 'OpenDungeonsIcons/SummonWorkerButton'
assert 'colourNavigationAtlas();\n    createSummonWorkerIcon();' in gui
shader = gui[gui.index('void shadeNavigationIcon('):gui.index('\nvoid colourNavigationAtlas(')]
symbol = gui[gui.index('void createSummonWorkerIcon('):gui.index('\nvoid createNavigationImages(')]
probe = r'''
#include <Ogre.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <vector>
namespace CEGUI {
struct Sizef {int width,height;Sizef(int w,int h):width(w),height(h){}};
struct Rectf {int left,top,right,bottom;Rectf(int a=0,int b=0,int c=0,int d=0):left(a),top(b),right(c),bottom(d){}};
struct Texture {enum PixelFormat{PF_RGBA};std::vector<unsigned char> pixels;int width=0,height=0;
 void loadFromMemory(const void* data,Sizef s,PixelFormat){width=s.width;height=s.height;
  const auto* p=static_cast<const unsigned char*>(data);pixels.assign(p,p+width*height*4);}};
struct Renderer {std::map<std::string,Texture> textures;Texture& createTexture(const std::string& name){return textures[name];}};
struct System {Renderer renderer;static System& getSingleton(){static System s;return s;}Renderer* getRenderer(){return &renderer;}};
struct BasicImage {Texture* texture=nullptr;Rectf area;void setTexture(Texture* t){texture=t;}void setArea(Rectf a){area=a;}};
struct ImageManager {std::map<std::string,BasicImage> images;static ImageManager& getSingleton(){static ImageManager m;return m;}
 BasicImage& get(const std::string& name){return images.at(name);}};
}
SHADER
SYMBOL
int main(int argc,char** argv){try{
 int checks=0,failures=0;auto check=[&](bool ok,const char* why){++checks;if(!ok){++failures;std::cout<<"FAIL "<<why<<'\n';}};
 auto& images=CEGUI::ImageManager::getSingleton();CEGUI::Texture original;
 images.images["OpenDungeonsIcons/SummonWorkerButton"].texture=&original;
 images.images["OpenDungeonsIcons/Other"].texture=&original;createSummonWorkerIcon();
 auto& image=images.get("OpenDungeonsIcons/SummonWorkerButton");auto& texture=*image.texture;
 check(image.texture!=&original,"existing summon image replaced");
 check(images.get("OpenDungeonsIcons/Other").texture==&original,"other artwork untouched");
 check(texture.width==64&&texture.height==64,"normal icon dimensions");
 check(image.area.left==0&&image.area.top==0&&image.area.right==64&&image.area.bottom==64,"correct image rectangle");
 const auto& p=texture.pixels;auto alpha=[&](int x,int y){return p[(y*64+x)*4+3];};
 int opaque=0,partial=0;for(int y=0;y<64;++y)for(int x=0;x<64;++x){
  int a=alpha(x,y);opaque+=a==255;partial+=a>0&&a<255;
  if(x<6||x>=58||y<5||y>=59)check(a==0,"transparent padding, no portrait background");}
 check(opaque>450&&opaque<1500,"compact silhouette");check(partial>80,"supersampled smooth edges");
 for(auto point:{std::pair<int,int>{28,18},{9,22},{46,22},{28,34},{28,51}})
  {if(alpha(point.first,point.second)<=200)std::cout<<"sample "<<point.first<<","<<point.second<<" alpha="<<int(alpha(point.first,point.second))<<'\n';
   check(alpha(point.first,point.second)>200,"round head pointed ears and face remain");}
 for(auto point:{std::pair<int,int>{21,31},{35,31},{28,41},{28,48},{44,46}})
  check(alpha(point.first,point.second)==0,"eyes nose mouth and surrounding space remain open");
 for(int y=0;y<12;++y)for(int x=0;x<64;++x)check(alpha(x,y)==0,"no crown tufts or horns above head");
 for(int y=38;y<59;++y)for(int x=46;x<58;++x)check(alpha(x,y)==0,"no magical sparkle beside worker");
 check(p[(35*64+28)*4+2]>p[(35*64+28)*4],"existing blue enamel palette");
 Ogre::Root root("","","");root.loadPlugin(std::string(argv[2])+"/bin/Codec_STBI");
 Ogre::ResourceGroupManager::getSingleton().addResourceLocation(std::string(argv[1])+"/gui","FileSystem","General");
 Ogre::Image atlas;atlas.load("ODIcons.png","General");
 const int width=512,height=192;std::vector<unsigned char> preview(width*height*4,255);
 for(int i=0;i<width*height;++i){preview[i*4]=20;preview[i*4+1]=23;preview[i*4+2]=28;}
 for(int n=0;n<4;++n){std::vector<unsigned char> icon=p;
  if(n){for(int y=0;y<64;++y)for(int x=0;x<64;++x){auto c=atlas.getColourAt(n*64+x,320+y,0);int i=(y*64+x)*4;
   icon[i]=static_cast<unsigned char>(c.r*255+.5f);icon[i+1]=static_cast<unsigned char>(c.g*255+.5f);
   icon[i+2]=static_cast<unsigned char>(c.b*255+.5f);icon[i+3]=static_cast<unsigned char>(c.a*255+.5f);}
   const unsigned char colours[][3]={{234,137,80},{231,183,100},{248,206,88}};
   shadeNavigationIcon(icon,64,colours[n-1][0],colours[n-1][1],colours[n-1][2]);}
  for(int y=0;y<128;++y)for(int x=0;x<128;++x){int from=((y/2)*64+x/2)*4,to=((y+8)*width+n*128+x)*4;
   for(int c=0;c<3;++c)preview[to+c]=(icon[from+c]*icon[from+3]+preview[to+c]*(255-icon[from+3]))/255;}}
 for(int y=0;y<32;++y)for(int x=0;x<32;++x){int from=(y*2*64+x*2)*4,to=((y+152)*width+48+x)*4;
  for(int c=0;c<3;++c)preview[to+c]=(p[from+c]*p[from+3]+preview[to+c]*(255-p[from+3]))/255;}
 Ogre::Image output;output.loadDynamicImage(preview.data(),width,height,Ogre::PF_BYTE_RGBA);
 output.save(std::string(argv[1])+"/build/review-followups/summon-worker-symbol-preview.png");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''.replace('SHADER', shader).replace('SYMBOL', symbol)
with tempfile.TemporaryDirectory(prefix='odp-worker-symbol-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', f'/I{prefix / "include/OGRE"}',
                    'check.cpp', '/Fecheck.exe', '/link', f'/LIBPATH:{prefix / "lib"}', 'OgreMain.lib'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe'), str(repo), str(prefix)], cwd=work, check=True)
