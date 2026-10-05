"""Structure check: attaching/detaching an entity node must not touch Ogre when the parent state already matches."""
from pathlib import Path
import re
import sys

repo = Path(__file__).resolve().parents[2]
text = (repo / 'source/render/RenderManager.cpp').read_text(encoding='utf-8')


def body(name):
    match = re.search(r'void RenderManager::' + name + r'\(.*?\n\}\n', text, re.S)
    if match is None:
        print('FAIL missing ' + name)
        sys.exit(1)
    return match[0]


failures = []
attach = body('rrAttachEntity')
detach = body('rrDetachEntity')
if 'entityNode->getParent() == parentNode' not in attach or attach.index('getParent() == parentNode') > attach.index('addChild'):
    failures.append('rrAttachEntity must check the current parent before addChild')
if 'entityNode->getParent() == parentNode' not in detach or detach.index('getParent() == parentNode') > detach.index('removeChild'):
    failures.append('rrDetachEntity must check the current parent before removeChild')
for failure in failures:
    print('FAIL ' + failure)
if failures:
    sys.exit(1)
print('OK scene node attach guard')
