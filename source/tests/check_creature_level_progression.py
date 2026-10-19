"""Check production stat/level wiring and XP tables; compile the behavior probe only with --probe."""
from pathlib import Path
from decimal import Decimal
import re
import subprocess
import tempfile
import math
import sys

root = Path(__file__).resolve().parents[2]
source = (root / 'source/entities/Creature.cpp').read_text()


# Extract production methods for source assertions and the optional integrator probe.
def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


config = (root / 'config/creatures.cfg').read_text()
definitions = re.findall(r'\[Creature\](.*?)\[/Creature\]', config, re.S)
cases = []
checks = 0
for definition in definitions:
    xp = re.search(r'\[XP\](.*?)\[/XP\]', definition, re.S)
    if not xp:
        continue
    values = [Decimal(v) for line in xp[1].splitlines() if not line.strip().startswith('#') for v in line.split()]
    assert len(values) == 29
    assert all(b > a > 0 for a, b in zip(values, values[1:]))
    checks += 29
    # Read the stat columns retained by both the Python checks and the optional probe.
    def stat(name):
        match = re.search(r'^\s*' + re.escape(name) + r'\s+([\d.]+)', definition, re.M)
        return float(match[1]) if match else 0
    cases.append('{' + ','.join(map(str, (stat('MinHP'), stat('HP/Level'), stat('PhysicalDefense'), stat('PhysicalDef/Level')))) + '}')

methods = '\n'.join(function(source, sig) for sig in ('void Creature::buildStats()', 'void Creature::setLevel(',
    'void Creature::checkLevelUp()', 'void Creature::receiveExp('))

sys.path.insert(0, str(root / 'tools/balance'))
from combat_sim import Roster, combat_stat, multiplier

build_stats = function(source, 'void Creature::buildStats()')
assert 'CreatureProgression::hpMultiplier' in build_stats
assert build_stats.count('CreatureProgression::damageMultiplier') == 3
assert build_stats.count('CreatureProgression::combatStat(') == 4
for member, getter in (('mDigRate', 'getDigRatePerLevel'), ('mClaimRate', 'getClaimRatePerLevel'),
                       ('mGroundSpeed', 'getGroundSpeedPerLevel'), ('mWaterSpeed', 'getWaterSpeedPerLevel'),
                       ('mLavaSpeed', 'getLavaSpeedPerLevel')):
    assert member + ' += mDefinition->' + getter + '() * multiplier;' in build_stats
set_level = function(source, 'void Creature::setLevel(')
for needle in ('std::max(1u, std::min(MAX_LEVEL, level))', 'mExp = 0.0;', 'buildStats();',
               'if(mHp > 0.0 && previousMaxHP > 0.0)', 'mHp * mMaxHP / previousMaxHP'):
    assert needle in set_level, needle
level_up = function(source, 'void Creature::checkLevelUp()')
for needle in ('while(getLevel() < MAX_LEVEL)', '!std::isfinite(newXP) || newXP <= 0.0',
               'if(mExp < newXP)', 'remainingXP = mExp - newXP', 'setLevel(mLevel + 1);', 'mExp = remainingXP;'):
    assert needle in level_up, needle
receive = function(source, 'void Creature::receiveExp(')
assert '!std::isfinite(experience) || experience < 0 || mLevel >= MAX_LEVEL' in receive
assert 'mExp += experience;' in receive
roster = Roster(root)
for name in roster.fighters() + ['Kobold', 'DwarfWorker']:
    fields = roster.creatures[name][0]
    hp = defense = 0
    for level in range(1, 31):
        unit = roster.unit(name, level)
        assert unit.hp > hp and unit.defense[0] >= defense, (name, level)
        assert unit.hp >= roster.number(fields, 'MinHP') + (level - 1) * roster.number(fields, 'HP/Level')
        assert multiplier(level, 'damage') >= 1
        hp, defense = unit.hp, unit.defense[0]
for old_hp in (0, -10, 40):
    new_max = combat_stat(100, 2, 30, 'hp')
    new_hp = min(new_max, old_hp * new_max / 100) if old_hp > 0 else old_hp
    assert new_hp == old_hp if old_hp <= 0 else math.isclose(new_hp / new_max, .4)
assert combat_stat(100, 2, 0, 'hp') == 100
assert combat_stat(100, 2, 999, 'hp') == combat_stat(100, 2, 30, 'hp')
print(f'XP_TABLE_CHECKS={checks} SPECIES={len(cases)}; stat and level source checks passed')
if '--probe' not in sys.argv:
    sys.exit(0)
damage = ''
for name, filename in [('melee', 'CreatureSkillMeleeFight.cpp'), ('ranged', 'CreatureSkillMissileLaunch.cpp')]:
    skill = (root / 'source/creatureskill' / filename).read_text()
    start = skill.index('    const uint32_t level = creature->getLevel();')
    stop = skill.index('    if(creature->getWeaponL()', start)
    damage += 'std::vector<double> ' + name + '(Creature* creature){\n' + skill[start:stop] + '\nreturn {phyAtk,magAtk,eleAtk};}\n'

probe = r'''
#include "entities/CreatureProgression.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
#define OD_LOG_ERR(x) ((void)0)
constexpr unsigned MAX_LEVEL=30;
struct Definition {
 double hp=100,hpStep=2,defense=3,defenseStep=.1,needed=100;
 double getMinHp(){return hp;}double getHpPerLevel(){return hpStep;}
 double getPhysicalDefense(){return defense;}double getPhysicalDefPerLevel(){return defenseStep;}
 double getMagicalDefense(){return defense;}double getMagicalDefPerLevel(){return defenseStep;}
 double getElementDefense(){return defense;}double getElementDefPerLevel(){return defenseStep;}
 double getDigRate(){return 10;}double getDigRatePerLevel(){return .5;}
 double getClaimRate(){return 2;}double getClaimRatePerLevel(){return .1;}
 double getMoveSpeedGround(){return 1;}double getGroundSpeedPerLevel(){return .02;}
 double getMoveSpeedWater(){return 0;}double getWaterSpeedPerLevel(){return 0;}
 double getMoveSpeedLava(){return 0;}double getLavaSpeedPerLevel(){return 0;}
 double getXPNeededWhenLevel(unsigned level){return needed*level;}
};
struct Creature {
 Definition definition;Definition* mDefinition=&definition;unsigned mLevel=1;double mExp=0,mHp=100,mMaxHP=100;
 double mDigRate=0,mClaimRate=0,mGroundSpeed=0,mWaterSpeed=0,mLavaSpeed=0,mPhysicalDefense=0,mMagicalDefense=0,mElementDefense=0;
 bool mNeedFireRefresh=false;
 unsigned getLevel(){return mLevel;}std::string getName(){return "probe";}
 void buildStats();void setLevel(unsigned);void checkLevelUp();void receiveExp(double);
};
METHODS
const double mPhyAtk=10,mPhyAtkPerLvl=.1,mMagAtk=3,mMagAtkPerLvl=.05,mEleAtk=2,mEleAtkPerLvl=.03;
DAMAGE
int main(){int checks=0,failures=0;auto check=[&](bool ok,const char* why){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<why<<'\n';}};
 check(CreatureProgression::powerMultiplier(1)==1&&CreatureProgression::powerMultiplier(30)==6,"power endpoints");
 check(CreatureProgression::powerMultiplier(0)==1&&CreatureProgression::powerMultiplier(999)==6,"power bounds");
 Creature fighter;auto firstMelee=melee(&fighter),firstRanged=ranged(&fighter);
 for(unsigned level=1;level<=30;++level){fighter.mLevel=level;auto m=melee(&fighter),r=ranged(&fighter);for(unsigned channel=0;channel<3;++channel){
  check(std::abs(m[channel]-firstMelee[channel]*CreatureProgression::powerMultiplier(level))<1e-8,"production melee channels scale");
  check(std::abs(r[channel]-firstRanged[channel]*CreatureProgression::powerMultiplier(level))<1e-8,"production ranged channels scale");}}
 double inputs[][4]={CASES};
 for(auto& data:inputs){Creature c;c.definition.hp=data[0];c.definition.hpStep=data[1];c.definition.defense=data[2];c.definition.defenseStep=data[3];
  double hp=0,def=-1,power=0;
  for(unsigned level=1;level<=30;++level){c.mLevel=level;c.buildStats();
   check(c.mMaxHP>hp,"health increases every level");check(c.mPhysicalDefense>=def,"defense never decreases");
   check(c.mMaxHP+1e-8>=data[0]+data[1]*(level-1),"configured HP growth retained");
   check(std::abs(c.mGroundSpeed-(1+.02*(level-1)))<1e-9&&c.mWaterSpeed==0&&c.mLavaSpeed==0,"movement unchanged");
   auto p=CreatureProgression::powerMultiplier(level);check(p>power,"attack power increases every level");power=p;hp=c.mMaxHP;def=c.mPhysicalDefense;
  }
 }
 Creature c;c.mHp=50;c.setLevel(30);check(c.mLevel==30&&std::abs(c.mMaxHP-100*CreatureProgression::hpMultiplier(30))<1e-9&&std::abs(c.mHp/c.mMaxHP-.5)<1e-9,"health fraction grows with vitality");
 c.mHp=0;c.setLevel(1);check(c.mHp==0,"level change cannot resurrect");c.mHp=-10;c.setLevel(30);check(c.mHp==-10,"KO state preserved");
 c.setLevel(0);check(c.mLevel==1,"editor cannot underflow level");c.setLevel(100);check(c.mLevel==30,"cap remains thirty");
 c.setLevel(1);c.mHp=40;c.receiveExp(650);c.checkLevelUp();check(c.mLevel==4&&c.mExp==50,"multiple levels retain surplus XP");
 check(std::abs(c.mHp/c.mMaxHP-.4)<1e-9,"multiple level-ups preserve health fraction");
 c.setLevel(1);c.receiveExp(99);c.checkLevelUp();check(c.mLevel==1&&c.mExp==99,"below threshold");
 c.receiveExp(1);c.checkLevelUp();check(c.mLevel==2&&c.mExp==0,"exact threshold");
 c.setLevel(1);c.receiveExp(1e9);c.checkLevelUp();check(c.mLevel==30&&c.mExp==0,"large grant stops at cap");c.receiveExp(100);check(c.mExp==0,"cap does not accumulate unusable XP");
 c.setLevel(1);for(double bad:{-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})c.receiveExp(bad);check(c.mExp==0,"invalid gains rejected");
 c.definition.needed=0;c.mExp=500;c.checkLevelUp();check(c.mLevel==1,"invalid threshold cannot loop");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''.replace('METHODS', methods).replace('DAMAGE', damage).replace('CASES', ','.join(cases))
with tempfile.TemporaryDirectory(prefix='odp-levels-') as directory:
    work = Path(directory)
    (work / 'probe.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', f'/I{root / "source"}', 'probe.cpp', '/Feprobe.exe'], cwd=work, check=True)
    subprocess.run([str(work / 'probe.exe')], cwd=work, check=True)
print(f'XP_TABLE_CHECKS={checks} SPECIES={len(cases)}')
