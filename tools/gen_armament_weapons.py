#!/usr/bin/env python3
"""Generate the five original armament meshes, shared surface and material."""
import math
import random
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
IRON, POLL, WOOD, GRIP, CRYSTAL = range(5)
UV = ((0.25, 0.125), (0.75, 0.125), (0.5, 0.375), (0.5, 0.625), (0.5, 0.875))

# Collect flat-shaded triangles from the specified primitive surfaces.
class Mesh:
    # Start an empty mesh.
    def __init__(self):
        self.vertices = []
        self.faces = []

    # Add a polygon facing away from its part centre, omitting degenerate triangles.
    def face(self, points, centre, band):
        for i in range(1, len(points) - 1):
            a, b, c = points[0], points[i], points[i + 1]
            u = tuple(b[j] - a[j] for j in range(3))
            v = tuple(c[j] - a[j] for j in range(3))
            n = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
            length = math.sqrt(sum(x*x for x in n))
            if length < 1e-14:
                continue
            if sum(n[j] * ((a[j]+b[j]+c[j])/3-centre[j]) for j in range(3)) < 0:
                b, c = c, b
                n = tuple(-x for x in n)
            base = len(self.vertices)
            self.vertices.extend((p, tuple(x/length for x in n), UV[band]) for p in (a, b, c))
            self.faces.append((base, base+1, base+2))

    # Join equal-sized cross-sections and cap both ends.
    def rings(self, rings, band):
        centre = tuple(sum(p[j] for r in rings for p in r)/sum(map(len, rings)) for j in range(3))
        self.face(list(reversed(rings[0])), centre, band)
        self.face(rings[-1], centre, band)
        for a, b in zip(rings, rings[1:]):
            for i in range(len(a)):
                k = (i+1) % len(a)
                self.face((a[i], a[k], b[k], b[i]), centre, band)

    # Add an axis-aligned box with the supplied bounds.
    def box(self, x0, x1, y0, y1, z0, z1, band):
        self.rings([[(x0,y0,z), (x1,y0,z), (x1,y1,z), (x0,y1,z)] for z in (z0,z1)], band)

    # Add an eight-sided surface through radius/height pairs.
    def prism(self, sections, band):
        self.rings([[(r*math.cos(i*math.pi/4),r*math.sin(i*math.pi/4),z) for i in range(8)] for z,r in sections], band)

    # Write Ogre XML with per-face normals and shared colour-band UVs.
    def write(self, path):
        root = ET.Element('mesh')
        sub = ET.SubElement(ET.SubElement(root,'submeshes'),'submesh', material='ArmamentWeapons', usesharedvertices='false', use32bitindexes='false', operationtype='triangle_list')
        faces = ET.SubElement(sub,'faces',count=str(len(self.faces)))
        for f in self.faces:
            ET.SubElement(faces,'face',**dict(zip(('v1','v2','v3'),map(str,f))))
        vb = ET.SubElement(ET.SubElement(sub,'geometry',vertexcount=str(len(self.vertices))),'vertexbuffer',positions='true',normals='true',texture_coords='1',texture_coord_dimensions_0='float2')
        for p,n,uv in self.vertices:
            vertex = ET.SubElement(vb,'vertex')
            for tag, values in (('position',p),('normal',n)):
                ET.SubElement(vertex,tag,**dict(zip(('x','y','z'),(format(x,'.9g') for x in values))))
            ET.SubElement(vertex,'texcoord',u=str(uv[0]),v=str(uv[1]))
        ET.indent(root)
        ET.ElementTree(root).write(path,encoding='utf-8',xml_declaration=True)

# Construct only the exact parts specified for each weapon.
def build(name):
    m = Mesh()
    if name == 'HandAxe':
        m.box(-.011,.011,-.011,.011,-.10,.25,WOOD)
        m.box(-.013,.013,-.013,.013,-.05,.05,GRIP)
        m.rings([[(-.006,.011,.17),(.006,.011,.17),(.006,.011,.25),(-.006,.011,.25)], [(-.0015,.10,.14),(.0015,.10,.14),(.0015,.10,.27),(-.0015,.10,.27)]],IRON)
        m.box(-.008,.008,-.031,-.011,.19,.23,POLL)
    elif name == 'Club':
        m.prism([(-.10,.022),(.06,.022),(.47,.05),(.489134,.046194),(.505355,.035355),(.516194,.019134),(.52,0)],WOOD)
        m.prism([(-.08,.025),(.04,.025)],GRIP)
        for z in (.36,.44):
            radius = .022+(.05-.022)*(z-.06)/(.47-.06)
            for angle in (0,120,240):
                angle = math.radians(angle)
                x,y = radius*math.cos(angle),radius*math.sin(angle)
                m.box(x-.007,x+.007,y-.007,y+.007,z-.007,z+.007,POLL)
    elif name == 'Trident':
        m.prism([(-.70,.014),(.55,.014)],WOOD)
        m.prism([(-.06,.017),(.06,.017)],GRIP)
        m.box(-.007,.007,-.070,.070,.55,.58,IRON)
        for y,top in ((0,.71),(-.06,.68),(.06,.68)):
            m.rings([[(-.005,y-.005,.58),(.005,y-.005,.58),(.005,y+.005,.58),(-.005,y+.005,.58)],[(0,y,top)]*4],IRON)
    elif name == 'RitualDagger':
        m.prism([(-.05,.012),(.05,.012)],GRIP)
        m.box(-.006,.006,-.030,.030,.05,.06,IRON)
        rings = []
        for i in range(6):
            t=i/5
            y,z,w=.03*t*t,.06+.15*t,.015*(1-t)
            rings.append([(-.003,y-w,z),(.003,y-w,z),(.003,y+w,z),(-.003,y+w,z)])
        m.rings(rings,IRON)
    elif name == 'Wand':
        m.prism([(-.05,.006),(.10,.006)],WOOD)
        m.prism([(-.03,.008),(.03,.008)],GRIP)
        m.rings([[(0,0,.10)]*4,[(.012,0,.11),(0,.012,.11),(-.012,0,.11),(0,-.012,.11)],[(0,0,.12)]*4],CRYSTAL)
    return m

# Fill the four specified colour bands with deterministic per-pixel variation.
def write_texture(path):
    random.seed(7)
    image = Image.new('RGB',(64,64))
    for y in range(64):
        for x in range(64):
            base = ((.64,.65,.67) if x<32 else (.39,.40,.42)) if y<16 else ((.48,.33,.18) if y<32 else ((.35,.21,.13) if y<48 else (.62,.53,.24)))
            noise = random.uniform(-.03,.03)
            image.putpixel((x,y),tuple(round(255*(v+noise)) for v in base))
    image.save(path)

# Generate XML in the private folder and the shared repository resources.
def main():
    out=Path(sys.argv[1])
    out.mkdir(parents=True,exist_ok=True)
    for name in ('HandAxe','Club','Trident','RitualDagger','Wand'):
        build(name).write(out/(name+'.mesh.xml'))
    write_texture(ROOT/'materials/textures/ArmamentWeaponsSurface.png')
    source=(ROOT/'materials/scripts/ShortSword.material').read_text()
    (ROOT/'materials/scripts/ArmamentWeapons.material').write_text(source.replace('ShortSword','ArmamentWeapons'))

if __name__ == '__main__':
    main()
