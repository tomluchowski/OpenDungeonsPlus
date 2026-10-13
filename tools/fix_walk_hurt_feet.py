#!/usr/bin/env python3
"""Re-author the clip WalkHurt (the limping walk) of an OGRE skeleton (XML form) from its Walk clip without sliding feet.

The Walk clip must already have locked feet (fix_walk_feet.py). WalkHurt keeps the length of Walk (the walk speed
factors depend on it), the same planted foot speed per clip second (so the clip rate of Walk fits, WalkHurtClipRate is
only needed where the measured value differs) and a closed loop. The limp is made of

  - the pose changes of gen_walk_hurt.py for the body (the body dips and rolls to the injured side, the spine leans
    forward, the head is lowered, the hand is pressed to the side, tails and wings droop),
  - a shorter stance of the injured foot (as far as the other feet still cover the same part of the cycle) and a lower
    swing arc (the foot is dragged),

and the leg chains of all feet are solved again by the IK of fix_walk_feet.py on the foot paths of the Walk clip, so every
planted foot moves backwards at one constant speed during its stance, whatever the body does.

usage:
  fix_walk_hurt_feet.py <skeleton.xml> <Rig> <walk meta.json> <out.xml> [--trim T] [--lift L] [--second-trim T]
                        [--second-lift L] [--pose-scale S] [--bob B] [--carry --carry-h H --carry-amp A] [--meta out.json]

<walk meta.json> is the meta file of the Walk clip (stance windows, ground speed per clip second) written by
fix_walk_feet.py fix. The output skeleton contains the new WalkHurt in place of the old one (appended when there is none).
"""
import sys, os, re, json, argparse
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fix_walk_feet as F
import gen_walk_hurt as G

CLIP = 'WalkHurt'
THIN = 0.03     # key spacing of the pose tracks that are not solved by the IK (seconds)


def feet_of(cfg, bones):
    bones = set(bones)
    return [k for k, f in enumerate(cfg['feet']) if bones.intersection(b for u in f['units'] for b in u['chain'])]


def make_pre(S, rigs, skel, cfg, scale=1.0, bob=0.6, body_tilt=False):
    for rig in rigs:
        if body_tilt and not rig.get('spine'):
            rig['spine'] = list(rig['body'])        # no spine bones: the whole body leans and rolls
        base = G.KIND_DEFAULTS[rig['kind']]
        p = dict(rig.get('p', {}))
        p['bob'] = bob
        for key in ('dip', 'lean', 'roll', 'head'):
            p[key] = p.get(key, base[key]) * scale
        rig['p'] = p
    skip = set(b for f in cfg['feet'] for u in f['units'] for b in u['chain'])

    def pre(loc, taus, tn):
        grid = [float(t) for t in tn]
        length, grid, P, base = G.build(S, rigs, None, grid=grid, skip=skip)
        touched = set()
        for b in S.order:
            T = np.array(P.T[b])
            Q = np.array(P.Q[b])
            T0 = np.array(base.T[b])
            Q0 = np.array(base.Q[b])
            dq = np.minimum(np.abs(Q - Q0).max(axis=1), np.abs(Q + Q0).max(axis=1)).max()
            if np.abs(T - T0).max() < 1e-7 and dq < 1e-7:
                continue
            T[-1] = T[0]
            Q[-1] = Q[0]
            i = skel.idx[b]
            p, q, s = loc[i]
            loc[i] = (skel.rest_pos[i][None] + T, F.qmul(np.tile(skel.rest_q[i], (len(T), 1)), Q), s)
            touched.add(b)
        return touched
    return pre


def hurt_feet(cfg, rigs, o):
    """foot index -> options of the limping feet"""
    out = {}
    for rig in rigs:
        for chain in rig.get('hurt', []):
            for k in feet_of(cfg, chain):
                out[k] = dict(trim=o['trim'], lift=o['lift'])
                if o.get('carry') and rig['kind'] in ('quad', 'multi'):
                    out[k]['carry'] = dict(h=o['carry_h'], amp=o['carry_amp'])
        for chain in rig.get('second', []):
            for k in feet_of(cfg, chain):
                if k not in out:
                    out[k] = dict(trim=o['second_trim'], lift=o['second_lift'])
    return out


def thin(t, T, Q, S, step):
    keep = [0]
    for j in range(1, len(t) - 1):
        if t[j] - t[keep[-1]] >= step:
            keep.append(j)
    keep.append(len(t) - 1)
    return t[keep], T[keep], Q[keep], S[keep]


def write_hurt(src_path, out_path, skel, fix, ik_bones):
    text = open(src_path, encoding='utf-8').read()
    walk = skel.clips['Walk']
    L = walk.length
    tn = fix['tn']
    names = list(walk.order) + [skel.names[i] for i in range(len(skel.names))
                                if skel.names[i] in fix['tracks'] and skel.names[i] not in walk.tracks]
    out = '<animation name="%s" length="%s">\n\t\t\t<tracks>' % (CLIP, F.fmt(L))
    for bone in names:
        if bone in fix['tracks']:
            T, Q, S = fix['tracks'][bone]
            tr = walk.tracks.get(bone)
            scale = tr.has_scale if tr is not None else False
            t = tn
            if bone not in ik_bones:
                t, T, Q, S = thin(tn, T, Q, S, THIN)
            for j in range(1, len(Q)):
                if np.dot(Q[j], Q[j - 1]) < 0:
                    Q[j] = -Q[j]
        else:
            tr = walk.tracks[bone]
            t, T, Q, S = tr.t, tr.T, tr.Q.copy(), tr.S
            for j in range(1, len(Q)):
                if np.dot(Q[j], Q[j - 1]) < 0:
                    Q[j] = -Q[j]
            scale = tr.has_scale
        out += '\n\t\t\t\t<track bone="%s">\n\t\t\t\t\t<keyframes>\n' % bone
        for j in range(len(t)):
            out += F.key_xml(t[j], T[j], Q[j], S[j], scale)
        out += '\t\t\t\t\t</keyframes>\n\t\t\t\t</track>'
    out += '\n\t\t\t</tracks>\n\t\t</animation>'
    m = re.search(r'<animation name="%s" length="[^"]+">' % CLIP, text)
    if m:
        a = m.start()
        e = text.index('</animation>', a) + len('</animation>')
        new = text[:a] + out + text[e:]
    else:
        i = text.rindex('</animations>')
        new = text[:i] + '\t\t' + out + '\n\t' + text[i:]
    open(out_path, 'w', encoding='utf-8', newline='\n').write(new)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('xml')
    ap.add_argument('rig')
    ap.add_argument('walkmeta')
    ap.add_argument('out')
    ap.add_argument('--trim', type=float, default=0.7)
    ap.add_argument('--lift', type=float, default=0.5)
    ap.add_argument('--second-trim', type=float, default=0.85)
    ap.add_argument('--second-lift', type=float, default=0.8)
    ap.add_argument('--pose-scale', type=float, default=1.5)
    ap.add_argument('--bob', type=float, default=0.8)
    ap.add_argument('--carry', action='store_true')
    ap.add_argument('--body-tilt', action='store_true')
    ap.add_argument('--carry-h', type=float, default=0.12)
    ap.add_argument('--carry-amp', type=float, default=0.05)
    ap.add_argument('--refine', type=int, default=3)
    ap.add_argument('--iters', type=int, default=30)
    ap.add_argument('--keydt', type=float, default=None)
    ap.add_argument('--wo', type=float, default=6.0)
    ap.add_argument('--no-homotopy', action='store_true')
    ap.add_argument('--no-repair', action='store_true')
    ap.add_argument('--wr', type=float, default=0.6)
    ap.add_argument('--extcap', type=float, default=0.95)
    ap.add_argument('--maxdrop', type=float, default=None)
    ap.add_argument('--meta', default=None)
    a = ap.parse_args()
    o = dict(trim=a.trim, lift=a.lift, second_trim=a.second_trim, second_lift=a.second_lift, carry=a.carry, carry_h=a.carry_h, carry_amp=a.carry_amp)
    if a.keydt:
        F.KEYDT = a.keydt
    cfg = F.load_rigs()[a.rig]
    skel = F.Skel(a.xml)
    S = G.Skeleton(a.xml)
    rigs = G.config(a.rig, S)
    if isinstance(rigs, str):
        print('SKIP', a.rig, rigs)
        return 1
    wm = json.load(open(a.walkmeta))
    walk = skel.clips['Walk']
    runs_fix = dict((k, [tuple(r) for r in f['runs']]) for k, f in enumerate(wm['feet']))
    hurt = hurt_feet(cfg, rigs, o)
    pre = make_pre(S, rigs, skel, cfg, a.pose_scale, a.bob, a.body_tilt)
    rate = cfg['v'] / wm['s_star']
    fx = F.fix_clip(skel, cfg, 'Walk', rate=rate, kappa=1.0, wo=a.wo, wr=a.wr, maxdrop=a.maxdrop, extcap=a.extcap,
                    refine=a.refine, iters=a.iters, hurt=hurt, pre=pre, runs_fix=runs_fix, d_fix=wm['d'], homotopy=not a.no_homotopy, repair=not a.no_repair)
    ik_bones = set(b for f in cfg['feet'] for u in f['units'] for b in u['chain'])
    for f in cfg['feet']:
        for u in f['units']:
            ik_bones.update(skel.parents_chain(u['chain'][0]))     # the bones the legs hang on keep every key
    write_hurt(a.xml, a.out, skel, fx, ik_bones)
    meta = dict(clip=CLIP, length=fx['Ln'], kappa=1.0, s_star=fx['s_star'], rate=rate, v=cfg['v'], d=wm['d'], H=wm['H'],
                body=fx['body'], dropmax=float(fx['drop'].max()),
                hurt=dict((str(k), dict(trim=h.get('used', 1.0), lift=h['lift'])) for k, h in hurt.items()),
                feet=[dict(bone=f['units'][0]['chain'][-1],
                           runs=[[s * fp.dt, ln * fp.dt] for (s, ln) in fp.runs])
                      for f, fp in zip(cfg['feet'], fx['A']['fps'])])
    json.dump(meta, open(a.meta or (a.out + '.json'), 'w'))
    print('WalkHurt %s length %.4f dropmax %.3f H  trims %s' % (a.rig, fx['Ln'], fx['drop'].max(),
          ' '.join('%d:%.2f' % (k, h.get('used', 1.0)) for k, h in sorted(hurt.items()))))
    vf = F.verify_fix(F.Skel(a.out), cfg, fx, CLIP)
    print('verify: rate %.3f slope err %.3f line dev %.4f H wdev %.3f lat %.4f H z %.4f H' % (vf['rate'], vf['slope'], vf['line'], vf['noise'], vf['lat'], vf['z']))
    for s in fx['stats']:
        print('foot %d unit %d  err max %.4f H mean %.4f H' % s)
    return 0


if __name__ == '__main__':
    sys.exit(main())
