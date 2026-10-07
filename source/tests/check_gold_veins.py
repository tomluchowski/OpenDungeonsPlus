"""Check the gold wall look: dark rock with gold veins and sparse glitter, no self-lighting, fully opaque.
Text check of the shader and material, no game start needed."""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding='utf-8')


frag = read('shaders/GoldDistortion.frag')
material = read('materials/scripts/Gold.material')
render = read('source/render/RenderManager.cpp')

# No area glow: the lit colour is never multiplied by the vein gain and nothing is added everywhere.
assert 'result * veinGain' not in frag and 'result *= veinGain' not in frag
# The gain stays at or below 1 so it can only scale the sheen and the glitter.
gain = float(re.search(r'param_named veinGain float ([0-9.]+)', material).group(1))
assert gain <= 1.0, 'veinGain above 1 makes the veins glow'
# Sheen and glitter are limited to the vein mask.
assert 'float vein = smoothstep' in frag
assert 'vein * veinGain * sheen' in frag
assert re.search(r'glitter\s*=\s*step\([^;]*\*\s*vein;', frag), 'glitter must be masked by the veins'
# Glitter is time dependent and sparse (only a few cells per area light up).
assert 'veinTime' in frag and 'step(0.90, cellRand)' in frag
# The whole gold texture receives the transparent material overlay, including its veins.
assert 'result = lightingTerm * texelColor;' in frag
assert 'Ogre::ColourValue markColor(0.65f, 0.45f, 1.0f, 0.5f)' in render
assert 'markPass->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA)' in render
assert 'markPass->setDepthWriteEnabled(false)' in render
# Output is clamped and opaque.
assert 'min(result, vec3(1.0))' in frag
assert re.search(r'color = vec4\([^;]*, 1\.0\);', frag)
assert 'scene_blend' not in material and 'alpha_rejection' not in material
assert 'emissive' not in material

print('check_gold_veins ok')
