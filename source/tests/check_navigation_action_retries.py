"""Exercise production failed-destination branches without starting a game."""
import argparse
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source-ref')
args = parser.parse_args()
handlers = []
for name in ('UseRoom', 'CarryEntity', 'ClaimWallTile', 'ClaimGroundTile', 'DigTile'):
    path = f'source/creatureaction/CreatureAction{name}.cpp'
    source = (subprocess.check_output(['git', 'show', args.source_ref + ':' + path], cwd=repo, text=True)
              if args.source_ref else (repo / path).read_text())
    start = source.index('if(!creature.setDestination(')
    end = source.index('return true;', start) + len('return true;')
    handlers.append(f'bool attempt{name}(Creature& creature){{Tile* dest=nullptr;Tile* tileDest=nullptr;Tile tileClaim,tilePos;\n' + source[start:end] + '\n}')

path = 'source/entities/Creature.cpp'
source = (subprocess.check_output(['git', 'show', args.source_ref + ':' + path], cwd=repo, text=True)
          if args.source_ref else (repo / path).read_text())
start = source.index('    if(setDestination(tileDest))', source.index('bool Creature::handleIdleAction()'))
end = source.index('\nbool Creature::searchBestTargetInList', start)
idle = source[start:end].rsplit('}', 1)[0].replace('setDestination(', 'creature.setDestination(')
handlers.append('bool attemptIdle(Creature& creature){Tile* tileDest=nullptr;\n' + idle + '\n}')

probe = r'''
#include <iostream>
#define OD_LOG_ERR(...) ((void)0)
struct Tile {};
struct Creature {
 bool reachable=false;int requests=0,popped=0;
 bool setDestination(Tile*){++requests;return reachable;}
 void popAction(){++popped;}
};
HANDLERS
int main(){
 int checks=0,failures=0;
 const auto check=[&](bool ok,const char* message){++checks;if(!ok){++failures;std::cout<<"FAIL "<<message<<'\n';}};
 for(auto handler:{attemptUseRoom,attemptCarryEntity,attemptClaimWallTile,attemptClaimGroundTile,attemptDigTile}){
  Creature creature;
  for(int iteration=0;iteration<20;++iteration)if(!handler(creature))break;
  check(creature.requests==1,"failed destination is not retried twenty times in the same tick");
  check(creature.popped==1,"failed assignment is released exactly once");
  handler(creature);
  check(creature.requests==2,"a later tick can try navigation again");
  creature=Creature{};creature.reachable=true;
  check(handler(creature)&&creature.requests==1&&creature.popped==0,"successful destination retains immediate walk-action processing");
 }
 Creature idle;
 for(int iteration=0;iteration<20;++iteration)if(!attemptIdle(idle))break;
 check(idle.requests==1,"failed idle wandering does not repeat pathfinding twenty times");
 check(idle.popped==0,"idle failure does not remove an unrelated action");
 attemptIdle(idle);check(idle.requests==2,"idle wandering can retry next tick");
 idle=Creature{};idle.reachable=true;
 check(!attemptIdle(idle)&&idle.requests==1,"successful idle walking still ends the tick");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''.replace('HANDLERS', '\n'.join(handlers))
with tempfile.TemporaryDirectory(prefix='odp-navigation-retry-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/O2', '/std:c++14', 'check.cpp', '/Fecheck.exe'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
