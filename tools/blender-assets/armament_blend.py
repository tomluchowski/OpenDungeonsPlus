"""Save one original weapon per background invocation in its private Blender file."""
from pathlib import Path
import os
import sys
import bpy

# Import the selected XML weapon into an empty scene and save its editable source.
def main():
    args=sys.argv[sys.argv.index('--')+1:]
    repo,private=map(Path,args[:2])
    name=os.environ['ODP_ARMAMENT_WEAPON']
    if name not in ('HandAxe','Club','Trident','RitualDagger','Wand'):
        raise ValueError('Unknown armament weapon')
    sys.path.insert(0,str(repo/'tools/blender-assets'))
    import odp_ogre_io
    bpy.ops.wm.read_factory_settings(use_empty=True)
    odp_ogre_io.import_mesh(str(private/'anim-work/armament/xml'/(name+'.mesh.xml')),None,name,str(repo/'materials/textures/ArmamentWeaponsSurface.png'))
    bpy.context.preferences.filepaths.save_version=0
    folder=private/'blender'
    folder.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(folder/('blender'+name+'.blend')))

if __name__ == '__main__':
    main()
