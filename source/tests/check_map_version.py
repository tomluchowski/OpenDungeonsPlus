"""Compile the actual map-listing/loading header gates, including supplied saves.

Run in the Windows developer environment with optional --save paths.
--source-ref HEAD tests the committed loader before a working-tree correction.
This does not start a game or exercise the later entity deserializers.
"""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source-ref")
parser.add_argument("--save", action="append", default=[])
args = parser.parse_args()
path = "source/gamemap/MapHandler.cpp"
source = (subprocess.check_output(["git", "show", f"{args.source_ref}:{path}"], cwd=root, text=True)
          if args.source_ref else (root / path).read_text(encoding="utf-8"))
helper = (root / "source/utils/Helper.cpp").read_text(encoding="utf-8")
reader = helper[helper.index("    bool readFile("):helper.index("    bool readNextLineNotEmpty(")]
cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
version = ".".join(re.search(r"set\(" + name + r"\s+(\d+)\)", cmake).group(1)
                   for name in ["OD_MAJOR_VERSION", "OD_MINOR_VERSION", "OD_PATCH_LEVEL"])


def header(function):
    start = source.index("    levelFile >> nextParam;", source.index(function))
    info = source.index('if (nextParam != "[Info]")', start)
    end = source.index("return false;", info) + len("return false;")
    following = end
    while source[following].isspace():
        following += 1
    if source[following] == "}":
        end = following + 1
    return source[start:end]


probe = r'''
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#define OD_LOG_WRN(message) ((void)0)
namespace ODApplication { const std::string VERSIONSTRING="OpenDungeons_Version:VERSION"; }
namespace Helper { READER }
bool load(std::stringstream& levelFile) { std::string nextParam; LOAD return true; }
bool list(std::stringstream& levelFile) { std::string nextParam; LIST return true; }
int main(int argc,char** argv) {
 int checks=0,failures=0;
 auto check=[&](bool result,const std::string& label){++checks;if(!result){++failures;std::cerr<<"FAIL "<<label<<"\n";}};
 for(const auto& version:{ODApplication::VERSIONSTRING,std::string("OpenDungeons_Version:0.7.1"),
     std::string("OpenDungeons_Version:0.6.0"),std::string("OpenDungeons_Version:99.0.0"),std::string("invalid")}) {
  const bool supported=version==ODApplication::VERSIONSTRING || version=="OpenDungeons_Version:0.7.1";
  for(bool validInfo:{false,true}) {
   std::stringstream input(version+(validInfo?"\n[Info]\n":"\n[Broken]\n"));
   check(load(input)==(supported && validInfo),"loader: "+version);
   input.clear();input.seekg(0);
   check(list(input)==(supported && validInfo),"listing: "+version);
  }
 }
 for(int i=1;i<argc;++i) {
  std::stringstream input;check(Helper::readFile(argv[i],input,true),"actual save is readable");
  check(load(input),"actual saved-game header passes loader");
  input.clear();input.seekg(0);check(list(input),"actual saved-game header passes listing");
 }
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<"\n";return failures?1:0;
}
'''
probe = probe.replace("OpenDungeons_Version:VERSION", "OpenDungeons_Version:" + version)
probe = probe.replace("LOAD", header("bool readGameMapFromFile("))
probe = probe.replace("LIST", header("bool getMapInfo("))
probe = probe.replace("READER", reader)
with tempfile.TemporaryDirectory(prefix="map-version-") as directory:
    work = Path(directory)
    cpp = work / "check.cpp"
    cpp.write_text(probe, encoding="utf-8")
    executable = work / "check.exe"
    subprocess.run(["cl", "/nologo", "/EHsc", "/std:c++14", str(cpp), f"/Fe:{executable}"],
                   cwd=work, check=True)
    subprocess.run([str(executable), *args.save], check=True)
