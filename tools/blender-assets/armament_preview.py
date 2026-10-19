"""Render the configured right-hand armament using the game's bone mount."""
from pathlib import Path
import json
import math
import os
import re
import sys
import xml.etree.ElementTree as ET
import bpy
import numpy as np
from mathutils import Matrix, Vector, Quaternion
from mathutils.bvhtree import BVHTree

sys.path.insert(0,str(Path(__file__).resolve().parent))
import odp_ogre_io
import wake_preview

# Find the named definition and read its existing mesh and weapon fields.
def definition(path, tag, name):
    for block in path.read_text().split('['+tag+']')[1:]:
        if re.search(r'^\s*Name\s+'+re.escape(name)+r'\s*$',block,re.M):
            return dict(re.findall(r'^\s*(MeshName|WeaponSpawnR|WeaponSpawnL)\s+(\S+)',block,re.M))
    raise ValueError('Missing definition '+name)

# Resolve the diffuse texture from the exact material declaration, including inherited materials.
def texture(repo, name):
    for path in sorted((repo/'materials/scripts').rglob('*.material')):
        text=re.sub(r'//[^\n]*','',path.read_text())
        match=re.search(r'^\s*material\s+'+re.escape(name)+r'(?=\s|\{|:)([^\{]*)\{',text,re.M)
        if match is None:
            continue
        start=match.end(); depth=1; end=start
        while depth:
            depth += (text[end]=='{')-(text[end]=='}')
            end+=1
        found=re.search(r'^\s*texture\s+(\S+)',text[start:end],re.M)
        if found:
            result=repo/'materials/textures'/found[1]
            if not result.exists():
                raise FileNotFoundError(result)
            return result
        parent=re.search(r':\s*(\S+)',match[1])
        if parent:
            return texture(repo,parent[1])
        raise ValueError('Material has no texture '+name)
    raise ValueError('Missing material '+name)

# Assign every submesh its own source texture rather than leaving extra materials untextured.
def texture_slots(repo, obj):
    for material in obj.data.materials:
        material.use_nodes=True
        tree=material.node_tree
        image=tree.nodes.new('ShaderNodeTexImage')
        image.image=bpy.data.images.load(str(texture(repo,material.name)),check_existing=True)
        tree.links.new(image.outputs['Color'],tree.nodes['Principled BSDF'].inputs['Base Color'])

# Normalize uniform bind scales in memory so Blender retains the original Ogre skin transforms.
def load_scaled(mesh_xml, skeleton_xml, image, name):
    reader=odp_ogre_io.read_skeleton
    original=reader(str(skeleton_xml))
    bones,parents,anims=original
    by_name={b['name']:b for b in bones}
    cumulative={}
    # Return the product of uniform rest scales from the root through this bone.
    def rest_scale(name):
        if name in cumulative:
            return cumulative[name]
        scale=by_name[name]['scale']
        if max(scale)-min(scale)>1e-6:
            raise ValueError('Nonuniform rest scale cannot be normalized: '+name)
        cumulative[name]=(rest_scale(parents[name]) if name in parents else 1)*scale[0]
        return cumulative[name]
    for bone in bones:
        rest_scale(bone['name'])
    # Supply normalized bind translations and animation offsets to the unchanged importer.
    def normalized(path):
        result=[]
        for bone in bones:
            copy=dict(bone)
            copy['pos']=bone['pos']*(cumulative[parents[bone['name']]] if bone['name'] in parents else 1)
            copy['scale']=Vector((1,1,1))
            result.append(copy)
        actions=[]
        for clip,length,tracks in anims:
            adjusted=[]
            for bone,keys in tracks:
                factor=cumulative[parents[bone]] if bone in parents else 1
                adjusted.append((bone,[(time,translation*factor,rotation,scale) for time,translation,rotation,scale in keys]))
            actions.append((clip,length,adjusted))
        return result,parents,actions
    odp_ogre_io.read_skeleton=normalized
    try:
        arm,obj=wake_preview.load(str(mesh_xml),str(skeleton_xml),str(image),name)
    finally:
        odp_ogre_io.read_skeleton=reader
    return arm,obj,original,cumulative

# Evaluate original Ogre local transforms independently to verify the preview's bone and skin matrices.
def verify_original(arm, original, cumulative, clip, seconds):
    bones,parents,anims=original
    source={bone['name']:bone for bone in bones}
    tracks=dict(next(tracks for name,length,tracks in anims if name==clip))
    bind,animated={},{}
    # Interpolate original XML key values at the rendered frame time.
    def sample(name):
        keys=tracks.get(name)
        if not keys:
            return Vector((0,0,0)),Quaternion((1,0,0,0)),Vector((1,1,1))
        if seconds<=keys[0][0]:
            return keys[0][1:]
        if seconds>=keys[-1][0]:
            return keys[-1][1:]
        for left,right in zip(keys,keys[1:]):
            if left[0]<=seconds<=right[0]:
                t=(seconds-left[0])/(right[0]-left[0])
                right_rotation=right[2].copy()
                if left[2].dot(right_rotation)<0:
                    right_rotation.negate()
                rotation=Quaternion(tuple((1-t)*left[2][i]+t*right_rotation[i] for i in range(4)))
                rotation.normalize()
                return left[1].lerp(right[1],t),rotation,left[3].lerp(right[3],t)
    # Compose the source hierarchy including its unmodified bind and animation scales.
    def world(name, poses, animate):
        if name in poses:
            return poses[name]
        bone=source[name]
        translation,rotation,scale=sample(name) if animate else (Vector((0,0,0)),Quaternion((1,0,0,0)),Vector((1,1,1)))
        local=Matrix.Translation(bone['pos']+translation) @ (bone['rot'] @ rotation).to_matrix().to_4x4() @ Matrix.Diagonal((*[bone['scale'][i]*scale[i] for i in range(3)],1))
        local=np.array(local,dtype=np.float64)
        poses[name]=(world(parents[name],poses,animate) @ local) if name in parents else local
        return poses[name]
    error=0
    worst=''
    for name in source:
        expected=world(name,animated,True)
        actual=np.array(arm.pose.bones[name].matrix,dtype=np.float64) @ np.diag((cumulative[name],cumulative[name],cumulative[name],1))
        reference_skin=expected @ np.linalg.inv(world(name,bind,False))
        preview_skin=np.array(arm.pose.bones[name].matrix,dtype=np.float64) @ np.linalg.inv(np.array(arm.data.bones[name].matrix_local,dtype=np.float64))
        difference=max(*(abs(actual[r][c]-expected[r][c]) for r in range(4) for c in range(4)),*(abs(preview_skin[r][c]-reference_skin[r][c]) for r in range(4) for c in range(4)))
        if difference>error:
            error,worst=difference,name
    # Blender reconstructs float bone frames; retain the measured residual and reject meaningful drift.
    if error>1e-4:
        raise ValueError('Original Ogre transform differs: '+clip+' '+worst+' '+str(error))
    return {'error':float(error),'bone':worst,'clip':clip,'seconds':seconds}

# Measure the Troll weapon's inward distances to the actual posed body surface for mount-trial comparison.
def troll_surface_distances(body, weapon, mount, parent_matrix):
    evaluated=body.evaluated_get(bpy.context.evaluated_depsgraph_get())
    mesh=evaluated.to_mesh()
    try:
        vertices=[evaluated.matrix_world @ vertex.co for vertex in mesh.vertices]
        tree=BVHTree.FromPolygons(vertices,[list(face.vertices) for face in mesh.polygons])
        inward=[]
        worst_point=None
        worst_vertex=None
        for vertex in weapon.data.vertices:
            point=weapon.matrix_world @ vertex.co
            nearest,normal,index,distance=tree.find_nearest(point)
            if nearest is not None and (point-nearest).dot(normal)<0:
                if not inward or distance>max(inward):
                    worst_point,worst_vertex=point.copy(),vertex.index
                inward.append(distance)
        crossings=[]
        if worst_point is not None:
            for vector in ((1,.173,.317),(-1,-.173,-.317),(.213,1,.419),(-.213,-1,-.419),(.137,.271,1),(-.137,-.271,-1)):
                direction=Vector(vector).normalized()
                start=worst_point.copy()
                hits=0
                for attempt in range(128):
                    hit,normal,index,distance=tree.ray_cast(start,direction,10)
                    if hit is None:
                        break
                    hits+=1
                    start=hit+direction*0.000001
                crossings.append(hits)
        origin_nearest,origin_normal,index,origin_distance=tree.find_nearest(mount.translation)
        escape=parent_matrix.inverted().to_3x3() @ (origin_nearest-mount.translation)
        return {'max_inward_distance':max(inward,default=0),'inside_vertex_count':len(inward),'grip_length':(mount.to_3x3() @ Vector((0,0,0.12))).length,'origin_nearest_surface_local_delta':list(escape),'worst_vertex':worst_vertex,'worst_point':list(worst_point) if worst_point is not None else None,'ray_crossings':crossings,'odd_ray_count':sum(count%2 for count in crossings)}
    finally:
        evaluated.to_mesh_clear()

# Return every evaluated creature and weapon vertex in world coordinates.
def world_vertices(objects):
    points=[]
    graph=bpy.context.evaluated_depsgraph_get()
    for obj in objects:
        evaluated=obj.evaluated_get(graph)
        mesh=evaluated.to_mesh()
        try:
            points.extend(evaluated.matrix_world @ vertex.co for vertex in mesh.vertices)
        finally:
            evaluated.to_mesh_clear()
    return points

# Project actual vertices using the unchanged camera lens and view angles.
def projected_bounds(points, elevation, azimuth, distance, target):
    e,a=math.radians(elevation),math.radians(azimuth)
    camera=Vector((math.sin(a)*math.cos(e),-math.cos(a)*math.cos(e),math.sin(e)))
    right=Vector((math.cos(a),math.sin(a),0))
    up=Vector((-math.sin(a)*math.sin(e),math.cos(a)*math.sin(e),math.cos(e)))
    projected=[]
    for point in points:
        delta=point-Vector(target)
        depth=distance-delta.dot(camera)
        if depth<=0:
            raise ValueError('Geometry behind preview camera')
        projected.append((0.5+delta.dot(right)/(0.6*depth),0.5-delta.dot(up)/(0.6*depth)))
    return [min(p[0] for p in projected),min(p[1] for p in projected),max(p[0] for p in projected),max(p[1] for p in projected)]

# Fit the combined posed bounding box with a ten-percent framing margin at the same view angle.
def full_camera(points, elevation, azimuth):
    low=Vector(tuple(min(p[i] for p in points) for i in range(3)))
    high=Vector(tuple(max(p[i] for p in points) for i in range(3)))
    target=(low+high)/2
    e,a=math.radians(elevation),math.radians(azimuth)
    camera=Vector((math.sin(a)*math.cos(e),-math.cos(a)*math.cos(e),math.sin(e)))
    right=Vector((math.cos(a),math.sin(a),0))
    up=Vector((-math.sin(a)*math.sin(e),math.cos(a)*math.sin(e),math.cos(e)))
    corners=[Vector((x,y,z))-target for x in (low.x,high.x) for y in (low.y,high.y) for z in (low.z,high.z)]
    distance=max(delta.dot(camera)+max(abs(delta.dot(right)),abs(delta.dot(up)))/0.27 for delta in corners)
    bounds=projected_bounds(points,elevation,azimuth,distance,target)
    if min(bounds[:2])<0.049 or max(bounds[2:])>0.951:
        raise ValueError('Full preview does not contain every actual vertex')
    return distance,target,bounds

# Render each available clip at both fractions and all three prescribed camera views.
def main():
    repo,out,name=sys.argv[sys.argv.index('--')+1:]
    repo,out=Path(repo),Path(out)
    xml=out.parent/'xml'
    creature=definition(repo/'config/creatures.cfg','Creature',name)
    equipment=definition(repo/'config/equipments.cfg','Equipment',creature['WeaponSpawnR'])
    if creature.get('WeaponSpawnL','none')!='none':
        raise ValueError('Preview assignment unexpectedly contains a left-hand item')
    mesh_xml=xml/(creature['MeshName']+'.xml')
    creature_mesh=ET.parse(mesh_xml).getroot()
    skeleton_xml=xml/(creature_mesh.find('skeletonlink').get('name')+'.xml')
    material=creature_mesh.find('submeshes/submesh').get('material')
    arm,obj,original,cumulative=load_scaled(mesh_xml,skeleton_xml,texture(repo,material),name)
    texture_slots(repo,obj)
    weapon_xml=xml/(equipment['MeshName']+'.xml')
    weapon_source=ET.parse(weapon_xml).getroot()
    weapon_material=weapon_source.find('submeshes/submesh').get('material')
    weapon_tip=max((Vector(tuple(float(p.get(axis)) for axis in ('x','y','z'))) for p in weapon_source.iter('position')),key=lambda point:point.z)
    weapon=odp_ogre_io.import_mesh(str(weapon_xml),None,'Weapon',str(texture(repo,weapon_material)))
    texture_slots(repo,weapon)
    weapon.parent=arm
    weapon.parent_type='BONE'
    weapon.parent_bone='Weapon_R'
    weapon.matrix_parent_inverse=Matrix.Identity(4)
    # Blender attaches at the bone tail; Ogre attaches at its origin.
    weapon.matrix_basis=Matrix.Translation((0,-arm.data.bones['Weapon_R'].length,0)) @ Matrix.Rotation(math.radians(-90),4,'X') @ Matrix.Scale(cumulative['Weapon_R'],4)
    positions=[float(p.get('z')) for p in creature_mesh.iter('position')]
    height=max(positions)-min(positions)
    clips={a.get('name'):float(a.get('length')) for a in ET.parse(skeleton_xml).getroot().findall('animations/animation')}
    report={'creature':name,'height':height,'weapon':equipment['MeshName'],'missing_clips':[],'images':[],'full_images':[],'troll_surface_samples':[],'mount_max_error':0,'original_matrix_max_error':0,'original_matrix_samples':[],'rest_scale':cumulative['Weapon_R'],'mount_samples':[]}
    out.mkdir(parents=True,exist_ok=True)
    arm.animation_data_create()
    for clip in ('Idle','Walk','Attack1','Attack2','Dragged'):
        if clip not in clips:
            report['missing_clips'].append(clip)
            continue
        for percent in (25,60):
            arm.animation_data.action=None
            for bone in arm.pose.bones:
                bone.location=(0,0,0)
                bone.rotation_quaternion=(1,0,0,0)
                bone.scale=(1,1,1)
            wake_preview.pose(arm,clip,clips[clip]*percent/100)
            proof=verify_original(arm,original,cumulative,clip,bpy.context.scene.frame_current/odp_ogre_io.FPS)
            report['original_matrix_samples'].append(proof)
            report['original_matrix_max_error']=max(proof['error'],report['original_matrix_max_error'])
            expected=arm.matrix_world @ arm.pose.bones['Weapon_R'].matrix @ Matrix.Scale(cumulative['Weapon_R'],4) @ Matrix.Rotation(math.radians(-90),4,'X')
            error=max(abs(weapon.matrix_world[r][c]-expected[r][c]) for r in range(4) for c in range(4))
            report['mount_max_error']=max(error,report['mount_max_error'])
            report['mount_samples'].append({'clip':clip,'percent':percent,'origin':list(expected.translation),'tip':list(expected @ weapon_tip),'hand':list((arm.matrix_world @ arm.pose.bones[arm.pose.bones['Weapon_R'].parent.name].matrix).translation)})
            if error>1e-5:
                raise ValueError('Bone mount differs from game transform: '+str(error))
            if name=='Troll':
                surface=troll_surface_distances(obj,weapon,expected,arm.matrix_world @ arm.pose.bones[arm.pose.bones['Weapon_R'].parent.name].matrix)
                surface.update({'clip':clip,'percent':percent})
                report['troll_surface_samples'].append(surface)
            points=world_vertices((obj,weapon))
            weapon_points=[weapon.matrix_world @ vertex.co for vertex in weapon.data.vertices]
            for elevation,azimuth in ((25,0),(70,0),(70,90)):
                filename=name+'_'+clip+'_'+str(percent)+'_e'+str(elevation)+'a'+str(azimuth)+'.png'
                if os.environ.get('ODP_ARMAMENT_FULL_ONLY')!='1':
                    wake_preview.render(str(out/filename),elevation=elevation,azimuth=azimuth,distance=3*height,target=(0,0,height/2))
                report['images'].append(str(out/filename))
                bounds=projected_bounds(weapon_points,elevation,azimuth,3*height,(0,0,height/2))
                if min(bounds[:2])<0 or max(bounds[2:])>1:
                    distance,target,full_bounds=full_camera(points,elevation,azimuth)
                    full=Path(filename).stem+'_full.png'
                    wake_preview.render(str(out/full),elevation=elevation,azimuth=azimuth,distance=distance,target=target)
                    report['full_images'].append({'file':str(out/full),'source':filename,'clip':clip,'percent':percent,'seconds':bpy.context.scene.frame_current/odp_ogre_io.FPS,'elevation':elevation,'azimuth':azimuth,'lens':60,'distance':distance,'target':list(target),'bounds':full_bounds,'actual_vertex_count':len(points),'source_weapon_bounds':bounds})
    (out.parent/'logs'/(name+'-preview.json')).write_text(json.dumps(report,indent=2))
    print(json.dumps(report),flush=True)

if __name__ == '__main__':
    try:
        main()
    except Exception:
        import traceback
        args=sys.argv[sys.argv.index('--')+1:]
        (Path(args[1]).parent/'logs'/(args[2]+'-preview-error.log')).write_text(traceback.format_exc())
        raise
