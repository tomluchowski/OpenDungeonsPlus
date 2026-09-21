"""Exercise the actual navigation shader without a GUI or game process."""
from pathlib import Path
import argparse
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-only', action='store_true')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[2]
source = (repo / 'source/render/Gui.cpp').read_text()
shader = source[source.index('void shadeNavigationIcon('):source.index('\nvoid colourNavigationAtlas(')]
code = r'''
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
SHADER
int main(){int checks=0,failures=0;auto check=[&](bool ok,const char* why){++checks;if(!ok){++failures;std::cout<<"FAIL "<<why<<'\n';}};
 for(int size:{16,32,64,128}){
  std::vector<unsigned char> pixels(size*size*4,0);
  for(int y=size/4;y<3*size/4;++y)for(int x=size/4;x<3*size/4;++x){
   const int i=(y*size+x)*4;pixels[i]=pixels[i+1]=pixels[i+2]=255;pixels[i+3]=255;
  }
  const int detail=(size/2*size+size/2)*4;
  pixels[detail]=pixels[detail+1]=pixels[detail+2]=0;
  const int coloured=(size/2*size+size/2+1)*4;
  pixels[coloured]=220;pixels[coloured+1]=50;pixels[coloured+2]=30;
  pixels[(size/4*size+size/4)*4+3]=128;
  const auto original=pixels;shadeNavigationIcon(pixels,size,120,185,235);
  for(int i=0;i<size*size;++i){
   check(pixels[i*4+3]==original[i*4+3],"silhouette and antialiasing remain exact");
   if(original[i*4+3]==0)check(std::equal(pixels.begin()+i*4,pixels.begin()+i*4+4,original.begin()+i*4),"transparent pixels remain untouched");
  }
  check(pixels[detail]==0&&pixels[detail+1]==0&&pixels[detail+2]==0,"black engraved details stay black");
  check(std::equal(pixels.begin()+coloured,pixels.begin()+coloured+4,original.begin()+coloured),"coloured artwork is not retinted");
  const int high=(size/4*size+size/2)*4,low=((3*size/4-1)*size+size/2)*4;
  check(pixels[high]>pixels[low]+80,"raised upper edge contrasts strongly with lower bevel");
  const int face=((size/2+1)*size+size/2)*4;
  check(pixels[face]<pixels[face+1]&&pixels[face+1]<pixels[face+2],"face retains category hue");
  auto repeated=original;shadeNavigationIcon(repeated,size,120,185,235);
  check(repeated==pixels,"identical inputs produce deterministic shading");
 }
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
'''.replace('SHADER', shader)
with tempfile.TemporaryDirectory(prefix='odp-navigation-shading-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(code)
    subprocess.run(['cl', '/nologo', '/EHsc', '/std:c++14', 'check.cpp', '/Fecheck.exe'], cwd=work, check=True)
    if args.compile_only:
        print('COMPILE ONLY: fixture built; runtime checks were not executed')
    else:
        subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
