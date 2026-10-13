# usage: splice_anims.py original.skeleton.xml exported.skeleton.xml out.skeleton.xml Name1 Name2 ...
import sys, re
orig, exp, out = sys.argv[1:4]
names = sys.argv[4:]
o = open(orig, encoding="utf-8").read()
e = open(exp, encoding="utf-8").read()
blocks = []
for n in names:
    m = re.search(r'\t\t<animation name="%s" length="[^"]*">.*?\n\t\t</animation>\n' % re.escape(n), e, re.S)
    if not m:
        raise SystemExit("clip %s not in export" % n)
    if ('<animation name="%s"' % n) in o:
        raise SystemExit("clip %s already exists in original" % n)
    blocks.append(m.group(0))
i = o.rindex("\t</animations>")
open(out, "w", encoding="utf-8", newline="\n").write(o[:i] + "".join(blocks) + o[i:])
print("spliced", names)
