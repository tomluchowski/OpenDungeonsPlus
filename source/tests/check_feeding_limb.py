"""Compile the production feeding limb solver against installed Ogre; no game/GUI."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
prefix = Path(os.environ["CMAKE_PREFIX_PATH"])
source = (root / "source/render/RenderManager.cpp").read_text(encoding="utf-8")
end = source.index('{', source.index('void solveFeedingLimb(')) + 1
depth = 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
solver = source[source.index("Ogre::Bone* findFeedingBone("):end]
probe = r'''
#include <Ogre.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
SOLVER
int checks = 0;
void check(bool value, const char* message)
{
    ++checks;
    if(!value) throw std::runtime_error(message);
}
int main()
{
    try {
        Ogre::Root engine("", "", "");
        Ogre::Skeleton rig(nullptr, "feeding-limb-test", 0, "General");
        auto* torso = rig.createBone("torso");
        auto* upper = rig.createBone("upper");
        auto* lower = rig.createBone("lower");
        auto* hand = rig.createBone("hand");
        auto* finger = rig.createBone("finger");
        torso->addChild(upper); upper->addChild(lower); lower->addChild(hand);
        hand->addChild(finger);
        upper->setPosition(0, 0, .6f);
        lower->setPosition(0, 0, -.25f);
        hand->setPosition(0, 0, -.2f);
        finger->setPosition(0, 0, -.06f);
        upper->setManuallyControlled(true); lower->setManuallyControlled(true);
        check(findFeedingBone(&rig, {"missing", "hand"}) == hand, "bone aliases resolve");
        check(findFeedingBone(&rig, {"absent"}) == nullptr, "missing limb is not invented");
        for(float angle : {0.f, 35.f, 90.f}) {
            torso->setOrientation(Ogre::Quaternion(Ogre::Degree(angle), Ogre::Vector3::UNIT_Z));
            for(const auto& offset : {Ogre::Vector3(0, -.2f, -.2f),
                    Ogre::Vector3(.2f, -.1f, -.1f), Ogre::Vector3(-.2f, .1f, .1f),
                    Ogre::Vector3(0, 0, -.44f), Ogre::Vector3(0, 0, .4f)}) {
                upper->setOrientation(Ogre::Quaternion::IDENTITY);
                lower->setOrientation(Ogre::Quaternion::IDENTITY);
                rig._updateTransforms();
                const auto target = upper->_getDerivedPosition() + offset;
                solveFeedingLimb(upper, lower, hand->getPosition(), target);
                check(hand->_getDerivedPosition().distance(target) < .0001f, "hand reaches target");
                check(std::abs(upper->_getDerivedPosition().distance(lower->_getDerivedPosition()) - .25f) < .0001f, "upper limb does not stretch");
                check(std::abs(lower->_getDerivedPosition().distance(hand->_getDerivedPosition()) - .2f) < .0001f, "lower limb does not stretch");
                check(rig.getManualBonesDirty(), "skin matrices remain invalidated for rendering");
                upper->setOrientation(Ogre::Quaternion::IDENTITY);
                lower->setOrientation(Ogre::Quaternion::IDENTITY);
                rig._updateTransforms();
                solveFeedingLimb(upper, lower, hand->getPosition() + finger->getPosition(), target);
                check(finger->_getDerivedPosition().distance(target) < .0001f, "finger grip reaches food rather than the wrist");
                check(std::abs(hand->_getDerivedPosition().distance(target) - .06f) < .0001f, "wrist stays clear of the food grip");
                check(std::abs(lower->_getDerivedPosition().distance(hand->_getDerivedPosition()) - .2f) < .0001f, "finger grip preserves forearm length");
            }
        }
        solveFeedingLimb(upper, lower, hand->getPosition(), Ogre::Vector3(0, -5, 0));
        check(std::abs(lower->getPosition().length() - .25f) < .0001f, "distant target preserves upper length");
        check(std::abs(hand->getPosition().length() - .2f) < .0001f, "distant target preserves lower length");
        std::cout << "CHECKS=" << checks << " FAILURES=0\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
'''.replace("SOLVER", solver)
with tempfile.TemporaryDirectory(prefix="odp-feeding-limb-") as temporary:
    directory = Path(temporary)
    cpp = directory / "check.cpp"
    cpp.write_text(probe, encoding="utf-8")
    subprocess.run(["cl.exe", "/nologo", "/EHsc", "/MD", "/std:c++14",
                    f"/I{prefix / 'include/OGRE'}", str(cpp), "/Fecheck.exe",
                    "/link", f"/LIBPATH:{prefix / 'lib'}", "OgreMain.lib"],
                   cwd=directory, check=True)
    subprocess.run([str(directory / "check.exe")], cwd=directory, check=True)
