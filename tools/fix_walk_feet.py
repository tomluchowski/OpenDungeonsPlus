#!/usr/bin/env python3
"""Re-author the Walk clip of an OGRE skeleton (XML form) so that planted feet do not slide.

Method (per foot): the contact point of the foot is followed through the original clip, the stance phases (foot low)
are found, and a new foot path is built: during every stance the foot moves backwards at one constant speed along the
walking axis (foot lock), during the swing it follows a smooth lifted arc to the next stance. The leg bones of the foot
are then solved with a damped least squares IK for every key so that the contact point follows the new path. The clip
may be stretched in time and the stride lengthened (--rho) so that the planted foot speed matches the ground speed.
Only the Walk animation block of the XML is replaced; bones, parents, rest pose and all other clips stay as they are.

usage:
  fix_walk_feet.py measure  <skeleton.xml> <Rig> [Clip]
  fix_walk_feet.py fix      <skeleton.xml> <Rig> <out.xml> [--rho R | --search] [--rate R] [--clip Walk]
                            [--duty D] [--extcap E] [--wo W] [--maxdrop M] [--maxerr X]
  fix_walk_feet.py carry    <old.xml> <new.xml> <out.xml>

fix: --search takes the longest stride (largest stride factor rho) whose IK error stays small and whose legs stay
bent (extension cap E of the chain length, default 0.95); --duty D < 1 shortens the stance windows (a jogging gait
with a short flight phase) and lengthens the cycle accordingly. carry: rebuilds the clip CarryWalk (the Walk clip with
other arm tracks) of a skeleton from its new Walk clip.

The rig tables (contact bones, IK chains, ground speed per clip second) are in fix_walk_feet_rigs.py next to this file.
"""
import sys, os, re, math, json, argparse
import xml.etree.ElementTree as ET
import numpy as np

# ----------------------------------------------------------------------------- quaternion / rotation helpers


def qmul(a, b):
    w1, x1, y1, z1 = a[..., 0], a[..., 1], a[..., 2], a[..., 3]
    w2, x2, y2, z2 = b[..., 0], b[..., 1], b[..., 2], b[..., 3]
    return np.stack([w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2,
                     w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
                     w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
                     w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2], axis=-1)


def qnorm(q):
    n = np.linalg.norm(q, axis=-1, keepdims=True)
    n = np.where(n == 0, 1, n)
    return q / n


def q2m(q):
    q = qnorm(q)
    w, x, y, z = q[..., 0], q[..., 1], q[..., 2], q[..., 3]
    m = np.empty(q.shape[:-1] + (3, 3))
    m[..., 0, 0] = 1 - 2 * (y * y + z * z)
    m[..., 0, 1] = 2 * (x * y - w * z)
    m[..., 0, 2] = 2 * (x * z + w * y)
    m[..., 1, 0] = 2 * (x * y + w * z)
    m[..., 1, 1] = 1 - 2 * (x * x + z * z)
    m[..., 1, 2] = 2 * (y * z - w * x)
    m[..., 2, 0] = 2 * (x * z - w * y)
    m[..., 2, 1] = 2 * (y * z + w * x)
    m[..., 2, 2] = 1 - 2 * (x * x + y * y)
    return m


def rv2q(v):
    """rotation vector (...,3) -> quaternion (...,4)"""
    a = np.linalg.norm(v, axis=-1, keepdims=True)
    h = a / 2
    s = np.where(a < 1e-12, 0.5, np.sin(h) / np.where(a < 1e-12, 1, a))
    return np.concatenate([np.cos(h), v * s], axis=-1)


def aa2q(angle, axis):
    axis = np.asarray(axis, float)
    n = np.linalg.norm(axis)
    if n == 0:
        return np.array([1.0, 0, 0, 0])
    axis = axis / n
    return np.concatenate([[math.cos(angle / 2)], axis * math.sin(angle / 2)])


def q2aa(q):
    q = qnorm(q)
    if q[0] < 0:
        q = -q
    s = math.sqrt(max(0.0, 1 - q[0] * q[0]))
    ang = 2 * math.atan2(s, q[0])
    if s < 1e-9:
        return 0.0, (1.0, 0.0, 0.0)
    return ang, tuple(q[1:] / s)


def err_rv(Rcur, Rref):
    """small rotation vector of Rcur * Rref^T (batched)"""
    E = Rcur @ np.swapaxes(Rref, -1, -2)
    return 0.5 * np.stack([E[..., 2, 1] - E[..., 1, 2], E[..., 0, 2] - E[..., 2, 0], E[..., 1, 0] - E[..., 0, 1]], axis=-1)


def slerp_arr(t, T, Q):
    """interpolate quaternion keys Q at times T for query times t (nlerp with sign fix; keys are dense)"""
    Q = Q.copy()
    for i in range(1, len(Q)):
        if np.dot(Q[i], Q[i - 1]) < 0:
            Q[i] = -Q[i]
    idx = np.clip(np.searchsorted(T, t, side='right') - 1, 0, len(T) - 2)
    t0 = T[idx]
    t1 = T[idx + 1]
    u = np.where(t1 > t0, (t - t0) / np.where(t1 > t0, t1 - t0, 1), 0.0)
    u = np.clip(u, 0, 1)[:, None]
    q0 = Q[idx]
    q1 = Q[idx + 1]
    d = np.sum(q0 * q1, axis=-1, keepdims=True)
    # slerp where angle is not tiny, else nlerp
    th = np.arccos(np.clip(d, -1, 1))
    sn = np.sin(th)
    use = sn > 1e-4
    a = np.where(use, np.sin((1 - u) * th) / np.where(use, sn, 1), 1 - u)
    b = np.where(use, np.sin(u * th) / np.where(use, sn, 1), u)
    return qnorm(a * q0 + b * q1)


# ----------------------------------------------------------------------------- skeleton / clips


class Track(object):
    def __init__(self, t, T, Q, S, has_scale):
        self.t = np.asarray(t, float)
        self.T = np.asarray(T, float)
        self.Q = np.asarray(Q, float)
        self.S = np.asarray(S, float)
        self.has_scale = has_scale

    def sample(self, t):
        t = np.asarray(t, float)
        T = np.stack([np.interp(t, self.t, self.T[:, i]) for i in range(3)], axis=-1)
        S = np.stack([np.interp(t, self.t, self.S[:, i]) for i in range(3)], axis=-1)
        Q = slerp_arr(t, self.t, self.Q) if len(self.t) > 1 else np.repeat(self.Q[:1], len(t), axis=0)
        return T, Q, S


class Clip(object):
    def __init__(self, name, length):
        self.name = name
        self.length = length
        self.tracks = {}   # bone -> Track (ordered by insertion in self.order)
        self.order = []


class Skel(object):
    def __init__(self, path):
        self.path = path
        root = ET.parse(path).getroot()
        self.names = []
        pos = []
        rq = []
        rs = []
        for b in root.find('bones').findall('bone'):
            self.names.append(b.get('name'))
            p = b.find('position')
            pos.append([float(p.get(c)) for c in 'xyz'])
            ro = b.find('rotation')
            if ro is not None:
                ax = ro.find('axis')
                rq.append(aa2q(float(ro.get('angle')), [float(ax.get(c)) for c in 'xyz']))
            else:
                rq.append(np.array([1.0, 0, 0, 0]))
            sc = b.find('scale')
            rs.append([float(sc.get(c)) for c in 'xyz'] if sc is not None else [1.0, 1, 1])
        self.idx = dict((n, i) for i, n in enumerate(self.names))
        n = len(self.names)
        self.parent = [-1] * n
        for bp in root.find('bonehierarchy').findall('boneparent'):
            self.parent[self.idx[bp.get('bone')]] = self.idx[bp.get('parent')]
        self.rest_pos = np.array(pos)
        self.rest_q = np.array(rq)
        self.rest_s = np.array(rs)
        # parents first
        done = set()
        self.topo = []

        def visit(i):
            if i in done:
                return
            if self.parent[i] >= 0:
                visit(self.parent[i])
            done.add(i)
            self.topo.append(i)
        for i in range(n):
            visit(i)
        self.clips = {}
        for a in root.find('animations').findall('animation'):
            c = Clip(a.get('name'), float(a.get('length')))
            for t in a.find('tracks').findall('track'):
                ts = []
                T = []
                Q = []
                S = []
                has_scale = False
                for k in t.find('keyframes').findall('keyframe'):
                    ts.append(float(k.get('time')))
                    tt = k.find('translate')
                    T.append([float(tt.get(cc)) for cc in 'xyz'] if tt is not None else [0.0, 0, 0])
                    ro = k.find('rotate')
                    if ro is not None:
                        ax = ro.find('axis')
                        Q.append(aa2q(float(ro.get('angle')), [float(ax.get(cc)) for cc in 'xyz']))
                    else:
                        Q.append(np.array([1.0, 0, 0, 0]))
                    sc = k.find('scale')
                    if sc is not None:
                        has_scale = True
                        S.append([float(sc.get(cc)) for cc in 'xyz'])
                    else:
                        S.append([1.0, 1, 1])
                c.tracks[t.get('bone')] = Track(ts, T, Q, S, has_scale)
                c.order.append(t.get('bone'))
            self.clips[c.name] = c

    def parents_chain(self, name):
        out = []
        i = self.idx[name]
        while i >= 0:
            out.append(self.names[i])
            i = self.parent[i]
        return out

    def locals_at(self, clip, times):
        """local (pos, q, s) arrays (N,3)/(N,4)/(N,3) of every bone at the given clip times"""
        N = len(times)
        out = []
        for i, nm in enumerate(self.names):
            tr = clip.tracks.get(nm)
            if tr is None:
                T = np.zeros((N, 3))
                Q = np.tile(np.array([1.0, 0, 0, 0]), (N, 1))
                S = np.ones((N, 3))
            else:
                T, Q, S = tr.sample(times)
            out.append((self.rest_pos[i] + T, qmul(np.tile(self.rest_q[i], (N, 1)), Q), self.rest_s[i] * S))
        return out

    def fk(self, loc):
        """world (p (N,3), R (N,3,3), s (N,3)) for every bone from local arrays"""
        W = [None] * len(self.names)
        for i in self.topo:
            p, q, s = loc[i]
            R = q2m(q)
            par = self.parent[i]
            if par < 0:
                W[i] = (p, R, s)
            else:
                pp, Rp, sp = W[par]
                W[i] = (pp + np.einsum('nij,nj->ni', Rp, sp * p), Rp @ R, sp * s)
        return W


def chain_fk(Wp, locs):
    """world transform of the chain bones; Wp = parent world (p, R, s) of the first chain bone (None for a root)"""
    out = []
    cur = Wp
    for p, q, s in locs:
        R = q2m(q)
        if cur is None:
            cur = (p, R, s)
        else:
            pp, Rp, sp = cur
            cur = (pp + np.einsum('nij,nj->ni', Rp, sp * p), Rp @ R, sp * s)
        out.append(cur)
    return out


# ----------------------------------------------------------------------------- foot analysis


def cyc_runs(mask):
    """contiguous cyclic runs of True in a boolean array -> list of (start, length) index pairs"""
    n = len(mask)
    if mask.all():
        return [(0, n)]
    if not mask.any():
        return []
    k0 = int(np.argmin(mask))   # first False
    runs = []
    i = 0
    while i < n:
        j = (k0 + i) % n
        if mask[j]:
            s = i
            while i < n and mask[(k0 + i) % n]:
                i += 1
            runs.append(((k0 + s) % n, i - s))
        else:
            i += 1
    return runs


class FootPath(object):
    """original contact point path of one foot with its stance runs"""

    def __init__(self, P, d, e, L, H=0.0, zfrac=0.4):
        self.P = P
        N = len(P)
        self.N = N
        self.L = L
        self.dt = L / N
        self.u = P @ d
        self.lat = P @ e
        self.z = P[:, 2]
        zr = self.z.max() - self.z.min()
        self.zr = zr
        self.zmin = self.z.min()
        low = (self.z - self.zmin) <= max(zfrac * zr, 0.02 * H)
        du = (np.roll(self.u, -1) - self.u) / self.dt
        runs = []
        if low.any():
            vref = float(np.median(du[low]))
            if vref < 0:
                back = (du <= 0.35 * vref) & ((self.z - self.zmin) <= 0.85 * zr)
                # close one or two sample gaps
                for _ in range(2):
                    back = back | (np.roll(back, 1) & np.roll(back, -1))
                for s, ln in cyc_runs(back):
                    idxs = [(s + k) % N for k in range(ln)]
                    if ln < 6 or not low[idxs].any():
                        continue
                    if abs(self.u[idxs[0]] - self.u[idxs[-1]]) < 0.03 * H:
                        continue
                    runs.append((s, ln))
        if not runs and (du < 0).any():
            # the low samples of this foot do not move backwards on average (it also creeps forward while low):
            # take the backward moving part of the cycle instead
            vneg = float(np.median(du[du < 0]))
            back = (du <= 0.35 * vneg) & ((self.z - self.zmin) <= 0.85 * zr)
            for _ in range(2):
                back = back | (np.roll(back, 1) & np.roll(back, -1))
            for s, ln in cyc_runs(back):
                idxs = [(s + k) % N for k in range(ln)]
                if ln < 6 or abs(self.u[idxs[0]] - self.u[idxs[-1]]) < 0.03 * H:
                    continue
                runs.append((s, ln))
            if len(runs) > 1:
                runs = [max(runs, key=lambda r: r[1])]
        if runs:
            mx = max(r[1] for r in runs)
            runs = [r for r in runs if r[1] >= 0.25 * mx]
        self.runs = runs


def walk_axis(paths):
    """unit horizontal direction in which the body walks (stance feet move backwards)"""
    sx = sy = 0.0
    for fp in paths:
        du = np.roll(fp.P, -1, axis=0) - fp.P
        low = (fp.z - fp.zmin) <= 0.2 * fp.zr
        w = np.where(low, 1.0 - (fp.z - fp.zmin) / (0.2 * fp.zr + 1e-12), 0.0)
        sx += np.sum(w * du[:, 0])
        sy += np.sum(w * du[:, 1])
    m = math.hypot(sx, sy)
    if m < 1e-12:
        return None
    return np.array([-sx / m, -sy / m])


# ----------------------------------------------------------------------------- the rig tables


def load_rigs():
    here = os.path.dirname(os.path.abspath(__file__))
    sys.path.insert(0, here)
    import fix_walk_feet_rigs as R
    return R.RIGS


class Unit(object):
    """one IK unit: chain of bones (top to tip), effector point on the last bone, optional translation DOF per bone"""

    def __init__(self, skel, spec):
        self.chain = list(spec['chain'])
        self.ids = [skel.idx[b] for b in self.chain]
        self.trans = [b in spec.get('trans', []) for b in self.chain]
        self.rot = [b not in spec.get('norot', []) for b in self.chain]
        eff = spec.get('eff')
        self.eff_off = np.array(eff, float) if eff is not None else np.zeros(3)
        self.hold = spec.get('hold', True)
        par = skel.parent[self.ids[0]]
        self.par = par
        self.nparam = sum(3 * int(r) + 3 * int(t) for r, t in zip(self.rot, self.trans))


def analyse(skel, cfg, clip, N=None):
    L = clip.length
    if N is None:
        N = max(480, int(math.ceil(L / 0.01)))
    taus = np.arange(N) * (L / N)
    loc = skel.locals_at(clip, taus)
    W = skel.fk(loc)
    paths = []
    for f in cfg['feet']:
        u0 = Unit(skel, f['units'][0])
        p, R, s = W[u0.ids[-1]]
        paths.append(p + np.einsum('nij,nj->ni', R, s * u0.eff_off))
    return taus, loc, W, paths


def body_height(skel):
    loc = [(skel.rest_pos[i][None], skel.rest_q[i][None], skel.rest_s[i][None]) for i in range(len(skel.names))]
    W = skel.fk(loc)
    zs = np.array([w[0][0][2] for w in W])
    return float(zs.max() - zs.min())


def foot_paths(skel, cfg, clip, d_fix=None):
    taus, loc, W, P = analyse(skel, cfg, clip)
    L = clip.length
    tmp = [FootPath(p, np.array([1.0, 0, 0]), np.array([0, 1.0, 0]), L) for p in P]
    d = walk_axis(tmp) if d_fix is None else np.array(d_fix, float)
    if d is None:
        return None
    d3 = np.array([d[0], d[1], 0.0])
    e3 = np.array([-d[1], d[0], 0.0])
    H = body_height(skel)
    P = list(P)
    for k, f in enumerate(cfg['feet']):
        c = f.get('copy')
        if not c:
            continue
        # a foot that hardly moves in the original: take the path of another foot (phase shift, mirrored sideways)
        src = c['from']
        sh = int(round(c.get('shift', 0.0) * len(P[k])))
        uk = np.roll(P[src] @ d3, sh)
        lk = np.roll(P[src] @ e3, sh)
        zk = np.roll(P[src][:, 2], sh)
        sg = -1.0 if c.get('mirror', True) else 1.0
        u2 = uk - np.mean(P[src] @ d3) + np.mean(P[k] @ d3)
        l2 = np.mean(P[k] @ e3) + sg * (lk - np.mean(P[src] @ e3))
        z2 = zk - np.min(P[src][:, 2]) + np.min(P[k][:, 2])
        P[k] = u2[:, None] * d3[None] + l2[:, None] * e3[None] + z2[:, None] * np.array([0, 0, 1.0])[None]
    fps = [FootPath(p, d3, e3, L, H) for p in P]
    return dict(taus=taus, loc=loc, W=W, P=P, fps=fps, d=d3, e=e3, H=H, L=L)


def measure(skel, cfg, clipname='Walk'):
    """planted foot speed per clip second, rate = ground speed / planted foot speed, deviation inside the stance"""
    clip = skel.clips[clipname]
    A = foot_paths(skel, cfg, clip)
    if A is None:
        return None
    v = cfg['v']
    res = []
    for k, fp in enumerate(A['fps']):
        vel_all = -(np.roll(fp.u, -1) - fp.u) / fp.dt
        for (s, ln) in fp.runs:
            idxs = np.array([(s + q) % fp.N for q in range(ln)])
            T = ln * fp.dt
            travel = fp.u[idxs[0]] - fp.u[idxs[-1]]
            vin = vel_all[idxs][int(0.1 * ln):max(int(0.1 * ln) + 1, ln - int(0.1 * ln))]
            med = float(np.median(vin))
            res.append(dict(foot=k, T=T, travel=travel, speed=travel / T, vmed=med,
                            dev=float(np.max(np.abs(vin - med)) / v), rms=float(np.std(vin) / v)))
    if not res:
        return None
    sp = sorted(r['speed'] for r in res)
    return dict(rate=v / sp[len(sp) // 2], H=A['H'], L=A['L'], runs=res, d=A['d'],
                stride=float(np.median([r['travel'] for r in res])), maxdev=max(r['dev'] for r in res),
                speeds=(sp[0], sp[-1]), cycles=len(res) / len(A['fps']), feet=len(A['fps']))


# ----------------------------------------------------------------------------- new foot paths


def new_path(fp, uc, latc, s_tau, taus, zground, lift=1.0):
    """new contact path (len(taus),3: u, lat, z in the walk frame) of one foot on the original clock tau"""
    L = fp.L
    runs = sorted(((s * fp.dt), ln * fp.dt) for (s, ln) in fp.runs)
    a0 = runs[0][0]
    A = [(r[0] - a0) % L for r in runs]
    Tn = [r[1] for r in runs]
    order = sorted(range(len(runs)), key=lambda i: A[i])
    A = [A[i] for i in order]
    Tn = [Tn[i] for i in order]
    n = len(A)
    ts = np.arange(fp.N) * fp.dt

    def orig(arr, tau):
        return np.interp(np.mod(tau, L), ts, arr, period=L)
    out = np.zeros((len(taus), 3))
    for j, tau in enumerate(taus):
        x = (tau - a0) % L
        done = False
        for i in range(n):
            if A[i] <= x <= A[i] + Tn[i]:
                u = uc - s_tau * (x - A[i] - Tn[i] / 2)
                out[j] = (u, latc, zground)
                done = True
                break
        if done:
            continue
        before = [k for k in range(n) if A[k] + Tn[k] < x]
        i = max(before) if before else n - 1
        endx = A[i] + Tn[i]
        nxt = (i + 1) % n
        startx = A[nxt] + (L if nxt == 0 else 0.0)
        if x < endx:
            x += L
        D = startx - endx
        w = (x - endx) / D
        uE = uc - s_tau * Tn[i] / 2
        uS = uc + s_tau * Tn[nxt] / 2
        m = -s_tau * D
        h00 = 2 * w ** 3 - 3 * w ** 2 + 1
        h10 = w ** 3 - 2 * w ** 2 + w
        h01 = -2 * w ** 3 + 3 * w ** 2
        h11 = w ** 3 - w ** 2
        u = h00 * uE + h10 * m + h01 * uS + h11 * m
        tE = a0 + endx
        tS = a0 + startx
        lat0 = orig(fp.lat, tau)
        lat = lat0 + (latc - orig(fp.lat, tE)) * (1 - w) + (latc - orig(fp.lat, tS)) * w
        z0 = orig(fp.z, tau) - fp.zmin
        zb = (orig(fp.z, tE) - fp.zmin) * (1 - w) + (orig(fp.z, tS) - fp.zmin) * w
        z = zground + lift * max(0.0, z0 - zb)
        out[j] = (u, lat, z)
    return out


# ----------------------------------------------------------------------------- IK


def solve_unit(unit, base, Wp, target, Rhold, H, wo=2.0, wr=0.6, iters=60, tol=0.0005, delta0=None):
    """batched damped least squares: returns delta (M,nparam), the final position error (M,) and the new locals"""
    M = target.shape[0]
    npar = unit.nparam
    off = unit.eff_off

    def build(delta):
        locs = []
        k = 0
        for b, (p, q, s) in enumerate(base):
            if unit.rot[b]:
                q = qmul(q, rv2q(delta[:, k:k + 3]))
                k += 3
            if unit.trans[b]:
                p = p + delta[:, k:k + 3]
                k += 3
            locs.append((p, q, s))
        return locs

    def resid(delta):
        W = chain_fk(Wp, build(delta))
        p, R, s = W[-1]
        x = p + np.einsum('nij,nj->ni', R, s * off)
        parts = [(x - target) / (tol * H)]
        if unit.hold:
            parts.append(err_rv(R, Rhold) * wo)
        parts.append(delta * wr)
        return np.concatenate(parts, axis=1), x
    delta = np.zeros((M, npar)) if delta0 is None else delta0.copy()
    r, x = resid(delta)
    cost = np.sum(r * r, axis=1)
    mu = np.full(M, 1e-2)
    eps = 1e-5
    for it in range(iters):
        J = np.empty((M, r.shape[1], npar))
        for k in range(npar):
            d2 = delta.copy()
            d2[:, k] += eps
            r2, _ = resid(d2)
            J[:, :, k] = (r2 - r) / eps
        JT = np.swapaxes(J, 1, 2)
        A = JT @ J
        g = np.einsum('nij,nj->ni', JT, r)
        step = np.linalg.solve(A + mu[:, None, None] * np.eye(npar)[None], -g[:, :, None])[:, :, 0]
        step = np.clip(step, -0.5, 0.5)
        d3 = delta + step
        r3, x3 = resid(d3)
        c3 = np.sum(r3 * r3, axis=1)
        ok = c3 < cost
        delta = np.where(ok[:, None], d3, delta)
        r = np.where(ok[:, None], r3, r)
        x = np.where(ok[:, None], x3, x)
        cost = np.where(ok, c3, cost)
        mu = np.where(ok, np.maximum(mu / 3, 1e-6), np.minimum(mu * 10, 1e6))
    err = np.linalg.norm(x - target, axis=1)
    return delta, err, build(delta)


def qconj(q):
    return np.array([q[0], -q[1], -q[2], -q[3]])


# ----------------------------------------------------------------------------- the fix


def body_bone(skel, cfg, clip):
    """bone whose translation track carries the hip height: cfg['body'] or the lowest common animated ancestor of the legs"""
    if 'body' in cfg:
        return cfg['body']
    chains = []
    for f in cfg['feet']:
        chains.append(skel.parents_chain(f['units'][0]['chain'][0]))
    common = [b for b in chains[0] if all(b in c for c in chains)]
    for b in common:
        if b in clip.tracks and b not in [u['chain'][0] for f in cfg['feet'] for u in f['units']]:
            return b
    return common[0] if common else None


def smooth_up(x, w):
    """cyclic max filter then moving average of the same half width w: the result stays at or above x"""
    mf = np.max([np.roll(x, s) for s in range(-w, w + 1)], axis=0)
    return np.mean([np.roll(mf, s) for s in range(-w, w + 1)], axis=0)


def stance_weight(fp, taus, L, ramp=0.15):
    """1 inside the stance windows of the foot (original clock), falling to 0 over `ramp` of the window length outside"""
    w = np.zeros(len(taus))
    for (s, ln) in fp.runs:
        a = s * fp.dt
        T = ln * fp.dt
        x = np.mod(taus - a, L)
        dist = np.where(x <= T, 0.0, np.minimum(x - T, L - x))
        w = np.maximum(w, np.clip(1.0 - dist / (ramp * T), 0.0, 1.0))
    return w


def sole_prep(skel, sole):
    """per sole vertex the bone weights with the vertex in the rest frame of each bone"""
    lr = [(skel.rest_pos[i][None], skel.rest_q[i][None], skel.rest_s[i][None]) for i in range(len(skel.names))]
    Wr = skel.fk(lr)
    prep = []
    for sv in sole:
        x = np.array(sv['p'])
        terms = []
        for b, w in sv['w'].items():
            i = skel.idx[b]
            p0, R0, s0 = Wr[i]
            terms.append((i, w, (R0[0].T @ (x - p0[0])) / s0[0]))
        prep.append(terms)
    return prep


def sole_mean(Wt, prep):
    """mean position (N,3) of the skinned sole vertices for the world transforms Wt of all bones"""
    N = Wt[0][0].shape[0]
    acc = np.zeros((N, 3))
    for terms in prep:
        for (i, w, xl) in terms:
            pt, Rt, st = Wt[i]
            acc += w * (pt + np.einsum('nij,nj->ni', Rt, st * xl[None]))
    return acc / len(prep)


def trim_runs(runs, N, f):
    """stance windows shortened to the fraction f of their length, around their centre"""
    out = []
    for (s, ln) in runs:
        ln2 = max(6, int(round(ln * f)))
        out.append(((s + (ln - ln2) // 2) % N, ln2))
    return out


def stance_mask(fp):
    m = np.zeros(fp.N, bool)
    for (s, ln) in fp.runs:
        m[[(s + q) % fp.N for q in range(ln)]] = True
    return m


def repair_jumps(un, base, Wp, tgt, Rhold, H, wo, wr, iters, locs, err, rounds=8):
    """keys of a chain solution that break the continuity of their neighbours (single keys that landed on a different leg
    configuration) are solved again with the middle of the neighbouring solutions as the prior. Returns locs, err"""
    n = len(tgt)
    for _ in range(rounds):
        dev = np.zeros(n)
        mids = []
        for (p, q, s) in locs:
            q = q.copy()
            for i in range(1, n):
                if np.dot(q[i], q[i - 1]) < 0:
                    q[i] = -q[i]
            nb = qnorm(np.roll(q, 1, axis=0) + np.roll(q, -1, axis=0))
            c = np.clip(np.abs(np.sum(nb * q, axis=1)), 0, 1)
            dev = np.maximum(dev, 2 * np.degrees(np.arccos(c)))
            mids.append(nb)
        med = float(np.median(dev))
        bad = dev > max(4.0, 8.0 * med)
        bad[0] = bad[-1] = False
        if not bad.any():
            break
        ib = np.where(bad)[0]
        base2 = []
        for b, (p, q, s) in enumerate(base):
            q2 = q.copy()
            qm = mids[b][ib]
            sg = np.sign(np.sum(qm * locs[b][1][ib], axis=1, keepdims=True))
            sg[sg == 0] = 1.0
            q2[ib] = qm * sg
            base2.append((p, q2, s))
        d2, e2, l2 = solve_unit(un, [(b_[0][ib], b_[1][ib], b_[2][ib]) for b_ in base2], Wp[0][ib:ib + 0] if False else _sel(Wp, ib), tgt[ib], Rhold[ib], H, wo=wo, wr=wr, iters=iters)
        locs = [(np.where(bad[:, None], _put(p1, ib, p2), p1), np.where(bad[:, None], _put(q1, ib, q2_), q1), s1)
                for (p1, q1, s1), (p2, q2_, s2) in zip(locs, l2)]
        err = err.copy()
        err[ib] = e2
    return locs, err


def _sel(Wp, ib):
    if Wp is None:
        return None
    return (Wp[0][ib], Wp[1][ib], Wp[2][ib])


def _put(full, ib, part):
    out = full.copy()
    out[ib] = part
    return out


def fix_clip(skel, cfg, clipname='Walk', rate=1.0, rho=1.0, kappa=None, wo=2.0, wr=0.6, maxdrop=None, iters=30, duty=1.0, extcap=0.95, refine=0, hurt=None, pre=None, runs_fix=None, d_fix=None, homotopy=False, repair=False):
    clip = skel.clips[clipname]
    A = foot_paths(skel, cfg, clip, d_fix)
    m = measure(skel, cfg, clipname)
    v = cfg['v']
    L = clip.length
    if maxdrop is None:
        maxdrop = cfg.get('maxdrop', 0.08)
    s0 = v / m['rate']                      # original planted speed per clip second
    s_star = v / rate                       # wanted planted speed per (new) clip second
    if duty < 1.0:
        # shorter stance windows (a jogging gait with a short flight phase): the same stride takes more cycle time
        for fp in A['fps']:
            nr = []
            for (s, ln) in fp.runs:
                ln2 = max(6, int(round(ln * duty)))
                nr.append(((s + (ln - ln2) // 2) % fp.N, ln2))
            fp.runs = nr
    if runs_fix:
        # stance windows (start, length in clip seconds) of a clip that was authored with exact windows: no detection
        for k, fpk in enumerate(A['fps']):
            if k in runs_fix:
                fpk.runs = [(int(round(a_ / fpk.dt)) % fpk.N, int(round(T_ / fpk.dt))) for (a_, T_) in runs_fix[k]]
    lift_of = {}
    if hurt:
        # limping feet (fix_walk_hurt_feet.py): the stance window of the foot is shortened around its centre as far as the
        # other feet still cover the same part of the cycle as before, and the swing arc is lowered (lift)
        for k, h in hurt.items():
            if k >= len(A['fps']):
                continue
            lift_of[k] = h.get('lift', 1.0)
            fpk = A['fps'][k]
            if not fpk.runs or h.get('carry'):
                continue
            base = list(fpk.runs)
            c0 = np.any([stance_mask(f_) for f_ in A['fps']], axis=0).mean()
            best = 1.0
            tt = 1.0
            while tt > 0.3:
                tt -= 0.02
                fpk.runs = trim_runs(base, fpk.N, tt)
                if np.any([stance_mask(f_) for f_ in A['fps']], axis=0).mean() < c0 - 0.002:
                    break
                best = tt
            use = max(best, h.get('trim', 1.0))
            fpk.runs = trim_runs(base, fpk.N, use)
            h['used'] = use
    if kappa is None:
        kappa = rho * s0 / s_star / duty    # time stretch so that the stride becomes rho times the original
    Ln = L * kappa
    M = int(math.ceil(Ln / min(KEYDT, Ln / 60.0) - 1e-9))
    tn = np.arange(M + 1) * (Ln / M)        # new key times
    taus_p = tn / kappa                     # original clock
    taus_p[-1] = 0.0                        # closed loop: the last key is the first key
    s_tau = s_star * kappa
    H = A['H']
    d3 = A['d']
    e3 = A['e']
    loc_o = skel.locals_at(clip, taus_p)
    if repair and not pre:
        # the other tracks are written time scaled and closed to a loop (close_track): the legs are solved on those values, so
        # that a pop spread over the end of the clip cannot move the hips under the planted feet
        ik_names = set(b for f in cfg['feet'] for u in f['units'] for b in u['chain'])
        for i, nm in enumerate(skel.names):
            tr = clip.tracks.get(nm)
            if tr is None or nm in ik_names:
                continue
            t_, T_, Q_, S_ = close_track(tr, kappa, Ln)
            T2, Q2, S2 = Track(t_, T_, Q_, S_, tr.has_scale).sample(tn)
            loc_o[i] = (skel.rest_pos[i][None] + T2, qmul(np.tile(skel.rest_q[i], (len(tn), 1)), Q2), skel.rest_s[i][None] * S2)
    W_o0 = skel.fk(loc_o) if pre else None     # the feet keep the world orientation of the clip before the pose changes
    touched = pre(loc_o, taus_p, tn) if pre else set()
    W_o = skel.fk(loc_o)
    zground = min(fp.zmin for fp in A['fps'] if fp.runs)
    feet = []
    path_of = {}
    corr = {}
    for k, f in enumerate(cfg['feet']):
        fp = A['fps'][k]
        if not fp.runs:
            continue
        uc = float(np.mean([np.mean(fp.u[[(s + q) % fp.N for q in range(ln)]]) for (s, ln) in fp.runs]))
        latc = float(np.mean([np.mean(fp.lat[[(s + q) % fp.N for q in range(ln)]]) for (s, ln) in fp.runs]))
        cr = hurt[k].get('carry') if hurt and k in hurt else None
        if cr:
            # a limb that is carried: it hangs in the air and paddles a little with the gait, it never touches the ground
            ph = 2.0 * np.pi * taus_p / L
            npth = np.stack([uc + cr['amp'] * H * np.sin(ph), np.full(len(taus_p), latc),
                             zground + cr['h'] * H * (1.0 + 0.25 * np.cos(2.0 * ph))], axis=1)
            fp.runs = []
        else:
            npth = new_path(fp, uc, latc, s_tau, taus_p, zground, lift_of.get(k, 1.0))
        Pn = npth[:, 0:1] * d3[None] + npth[:, 1:2] * e3[None] + npth[:, 2:3] * np.array([0, 0, 1.0])[None]
        u0 = Unit(skel, f['units'][0])
        p0, R0, s0_ = W_o[u0.ids[-1]]
        P0 = p0 + np.einsum('nij,nj->ni', R0, s0_ * u0.eff_off)
        feet.append((k, f, Pn - P0))
        path_of[k] = Pn
    body = body_bone(skel, cfg, clip)
    bi = skel.idx[body] if body else None
    n = len(taus_p)
    units = []
    Delta_of = {}
    for (k, f, Delta) in feet:
        for ui, us in enumerate(f['units']):
            un = Unit(skel, us)
            pe, Re, se = W_o[un.ids[-1]]
            xe = pe + np.einsum('nij,nj->ni', Re, se * un.eff_off)
            units.append((k, ui, un, xe + Delta, W_o0[un.ids[-1]][1] if pre else Re))
            Delta_of[(k, ui)] = Delta

    cap_of = {}
    for (k, ui, un, target, Re) in units:
        # the original clip sets the cap when it already stretches the leg further
        Wc0 = chain_fk(W_o[un.par] if un.par >= 0 else None, [(loc_o[i][0], loc_o[i][1], loc_o[i][2]) for i in un.ids])
        Lc0 = np.zeros(n)
        for a_, b_ in zip(Wc0[:-1], Wc0[1:]):
            Lc0 += np.linalg.norm(b_[0] - a_[0], axis=1)
        Lc0 += np.linalg.norm(Wc0[-1][2] * un.eff_off[None], axis=1)
        e0 = np.linalg.norm(target - Delta_of[(k, ui)] - Wc0[0][0], axis=1) / np.maximum(Lc0, 1e-9)
        cap_of[(k, ui)] = max(extcap, float(e0.max()) + 0.005)

    def run(drop, idx):
        """solve every unit for the samples idx with the body bone lowered by drop (len(idx),)"""
        loc = [(p[idx], q[idx], s[idx]) for (p, q, s) in loc_o]
        if bi is not None and np.any(drop > 0):
            p, q, s = loc[bi]
            par = skel.parent[bi]
            g = np.zeros((len(idx), 3))
            g[:, 2] = -drop
            if par >= 0:
                pp, Rp, sp = skel.fk(loc)[par]
                gl = np.einsum('nji,nj->ni', Rp, g) / sp
            else:
                gl = g
            loc[bi] = (p + gl, q, s)
        W = skel.fk(loc)
        tracks = {}
        errs = []
        for (k, ui, un, target, Re) in units:
            base = [(loc[i][0], loc[i][1], loc[i][2]) for i in un.ids]
            Wp = W[un.par] if un.par >= 0 else None
            tgt = target[idx] + (corr[k][idx] if k in corr else 0.0)
            if homotopy:
                # homotopy from a stiff to the normal regularisation: every key stays on the leg configuration nearest to the
                # clip it is derived from (no jump to the mirrored knee configuration at single keys)
                delta = None
                for wr_k in (4.0, 1.5, wr):
                    delta, err, locs = solve_unit(un, base, Wp, tgt, Re[idx], H, wo=wo, wr=wr_k, iters=iters, delta0=delta)
            else:
                delta, err, locs = solve_unit(un, base, Wp, tgt, Re[idx], H, wo=wo, wr=wr, iters=iters)
            if repair and len(idx) == n:
                locs, err = repair_jumps(un, base, Wp, tgt, Re[idx], H, wo, wr, iters, locs, err)
            for b_, i_ in enumerate(un.ids):
                loc[i_] = locs[b_]
            viol = err
            if not any(un.trans) and extcap < 1.0:
                # a leg that is stretched out more than the cap counts like a position error (lowers the body)
                Wc = chain_fk(Wp, locs)
                Lc = np.zeros(len(idx))
                for a_, b_ in zip(Wc[:-1], Wc[1:]):
                    Lc += np.linalg.norm(b_[0] - a_[0], axis=1)
                Lc += np.linalg.norm(Wc[-1][2] * un.eff_off[None], axis=1)
                extn = np.linalg.norm(tgt - Wc[0][0], axis=1) / np.maximum(Lc, 1e-9)
                cap_u = cap_of[(k, ui)]
                viol = err + np.maximum(0.0, extn - cap_u) * Lc
            errs.append((k, ui, viol))
            for b, name in enumerate(un.chain):
                p, q, s = locs[b]
                i = un.ids[b]
                Q = qmul(np.tile(qconj(skel.rest_q[i]), (len(q), 1)), q)
                tracks[name] = (p - skel.rest_pos[i], Q, s / skel.rest_s[i])
        if bi is not None and np.any(drop > 0):
            p, q, s = loc[bi]
            Q = qmul(np.tile(qconj(skel.rest_q[bi]), (len(q), 1)), q)
            tracks[body] = (p - skel.rest_pos[bi], Q, s / skel.rest_s[bi])
        for name in touched:
            if name not in tracks:
                i = skel.idx[name]
                p, q, s = loc[i]
                Q = qmul(np.tile(qconj(skel.rest_q[i]), (len(q), 1)), q)
                tracks[name] = (p - skel.rest_pos[i], Q, s / skel.rest_s[i])
        return tracks, errs, loc
    tol = 0.0015 * H
    allidx = np.arange(n)
    cands = [0.0] + [H * x for x in (0.005, 0.01, 0.02, 0.03, 0.045, 0.06, 0.08, 0.1, 0.12) if x <= maxdrop + 1e-12]
    need = np.zeros(n)
    tracks, errs, _ = run(np.zeros(n), allidx)
    ok = np.all([e[2] <= tol for e in errs], axis=0)
    pending = ~ok
    for dv in cands[1:]:
        if not pending.any():
            break
        pidx = np.where(pending)[0]
        tr, er, _ = run(np.full(len(pidx), dv), pidx)
        ok2 = np.all([e[2] <= tol for e in er], axis=0)
        need[pidx[ok2]] = dv
        pending[pidx[ok2]] = False
    need[pending] = cands[-1]
    drop = need
    if drop.max() > 0:
        w = max(2, int(0.08 / (Ln / M)))
        drop = smooth_up(need[:-1], w)
        drop = np.concatenate([drop, drop[:1]])
        tracks, errs, _ = run(drop, allidx)
    if refine > 0:
        # final passes: keep the skinned sole (mean of the sole vertices) on the designed stance line
        preps = {}
        for (k, f, Delta) in feet:
            if f.get('sole'):
                preps[k] = sole_prep(skel, f['sole'])
        for _ in range(refine):
            tracks, errs, loc_f = run(drop, allidx)
            W_f = skel.fk(loc_f)
            for k, prep in preps.items():
                E = sole_mean(W_f, prep) - path_of[k]
                E[-1] = E[0]
                E = E * stance_weight(A['fps'][k], taus_p, L)[:, None] * 0.8
                corr[k] = corr[k] - E if k in corr else -E
        tracks, errs, loc_f = run(drop, allidx)
    stats = [(k, ui, float(e.max() / H), float(e.mean() / H)) for (k, ui, e) in errs]
    return dict(kappa=kappa, Ln=Ln, tn=tn, tracks=tracks, stats=stats, s_star=s_star, rate=rate, A=A, drop=drop / H, body=body)


def search(skel, cfg, clipname='Walk', rate=1.0, lo=0.5, hi=3.5, maxerr=0.004, steps=6, **kw):
    """largest stride factor rho (bisection) whose IK error stays below maxerr (fraction of the body height)"""
    best = None
    fx = fix_clip(skel, cfg, clipname, rate=rate, rho=lo, **kw)
    if max(s[2] for s in fx['stats']) > maxerr:
        return None, None
    best = (lo, fx)
    for _ in range(steps):
        mid = 0.5 * (lo + hi)
        fx = fix_clip(skel, cfg, clipname, rate=rate, rho=mid, **kw)
        e = max(s[2] for s in fx['stats'])
        print('rho %.3f kappa %.3f L %.3f err %.4f H drop max %.3f H' % (mid, fx['kappa'], fx['Ln'], e, fx['drop'].max()))
        if e <= maxerr:
            lo = mid
            best = (mid, fx)
        else:
            hi = mid
    return best


# ----------------------------------------------------------------------------- XML writing


KEYDT = 0.0075


def fmt(x):
    return '%.7g' % x


def key_xml(t, T, Q, S, scale):
    ang, ax = q2aa(Q)
    ind = '\t' * 6
    out = ind + '<keyframe time="%s">\n' % fmt(t)
    out += ind + '\t<translate x="%s" y="%s" z="%s" />\n' % (fmt(T[0]), fmt(T[1]), fmt(T[2]))
    out += ind + '\t<rotate angle="%s">\n' % fmt(ang)
    out += ind + '\t\t<axis x="%s" y="%s" z="%s" />\n' % (fmt(ax[0]), fmt(ax[1]), fmt(ax[2]))
    out += ind + '\t</rotate>\n'
    if scale:
        out += ind + '\t<scale x="%s" y="%s" z="%s" />\n' % (fmt(S[0]), fmt(S[1]), fmt(S[2]))
    out += ind + '</keyframe>\n'
    return out


def close_track(tr, kappa, Ln, win=0.2):
    """keys of an untouched track with the time scaled by kappa, the last key at the clip length and a closed loop (last key
    = first key): a small pop of the original is spread over the last `win` part of the clip"""
    t = tr.t * kappa
    T = tr.T.copy()
    Q = tr.Q.copy()
    S = tr.S.copy()
    for i in range(1, len(Q)):
        if np.dot(Q[i], Q[i - 1]) < 0:
            Q[i] = -Q[i]
    if t[-1] < Ln - 1e-6:
        t = np.concatenate([t, [Ln]])
        T = np.vstack([T, T[-1:]])
        Q = np.vstack([Q, Q[-1:]])
        S = np.vstack([S, S[-1:]])
    else:
        t = t.copy()
        t[-1] = Ln
    dT = T[0] - T[-1]
    dS = S[0] - S[-1]
    c = qmul(np.array([Q[-1][0], -Q[-1][1], -Q[-1][2], -Q[-1][3]]), Q[0])
    if c[0] < 0:
        c = -c
    if np.abs(dT).max() > 1e-6 or np.abs(dS).max() > 1e-6 or c[0] < 1 - 1e-9:
        w = np.clip((t - (Ln - win * Ln)) / (win * Ln), 0.0, 1.0)
        w = w * w * (3 - 2 * w)
        ang, ax = q2aa(c)
        for i in range(len(t)):
            T[i] = T[i] + w[i] * dT
            S[i] = S[i] + w[i] * dS
            if ang > 0:
                Q[i] = qmul(Q[i], aa2q(ang * w[i], ax))
        Q[-1] = Q[0]
        T[-1] = T[0]
        S[-1] = S[0]
    return t, T, Q, S


def write_xml(src_path, out_path, skel, clipname, fix):
    text = open(src_path, encoding='utf-8').read()
    m = re.search(r'<animation name="%s" length="[^"]+">' % re.escape(clipname), text)
    a = m.start()
    e = text.index('</animation>', a) + len('</animation>')
    block = text[a:e]
    kappa = fix['kappa']
    clip = skel.clips[clipname]
    body0 = block[block.index('<tracks>') + len('<tracks>'):block.rindex('</tracks>')]
    parts = re.split(r'(?=<track bone=")', body0)
    out = '<animation name="%s" length="%s">\n\t\t\t<tracks>' % (clipname, fmt(fix['Ln']))
    tn = fix['tn']
    n = len(tn)
    for part in parts:
        mm = re.match(r'<track bone="([^"]+)"', part)
        if not mm:
            out += part
            continue
        bone = mm.group(1)
        if bone in fix['tracks']:
            T, Q, S = fix['tracks'][bone]
            scale = clip.tracks[bone].has_scale
            nt = '<track bone="%s">\n\t\t\t\t\t<keyframes>\n' % bone
            for j in range(n):
                nt += key_xml(tn[j], T[j], Q[j], S[j], scale)
            nt += '\t\t\t\t\t</keyframes>\n\t\t\t\t</track>\n\t\t\t\t'
            out += nt
        else:
            tr = clip.tracks[bone]
            t, T, Q, S = close_track(tr, kappa, fix['Ln'])
            nt = '<track bone="%s">\n\t\t\t\t\t<keyframes>\n' % bone
            for j in range(len(t)):
                nt += key_xml(t[j], T[j], Q[j], S[j], tr.has_scale)
            nt += '\t\t\t\t\t</keyframes>\n\t\t\t\t</track>\n\t\t\t\t'
            out += nt
    out += '</tracks>\n\t\t</animation>'
    new = text[:a] + out + text[e:]
    open(out_path, 'w', encoding='utf-8', newline='\n').write(new)



def verify_fix(skel_new, cfg, fix, clipname='Walk'):
    """measure the written clip inside the designed stance windows: slope error of the planted foot (fraction of the
    wanted speed), largest deviation of the foot from its straight stance line (fraction of the body height), lateral
    drift and height wobble (fraction of the body height), velocity noise (rms, fraction of the ground speed)"""
    clip = skel_new.clips[clipname]
    A = fix['A']
    kappa = fix['kappa']
    Ln = clip.length
    N = max(1200, int(math.ceil(Ln / 0.005)))
    taus, loc, W, P = analyse(skel_new, cfg, clip, N)
    dt = Ln / N
    v = cfg['v']
    H = A['H']
    s_star = fix['s_star']
    worst = dict(slope=0.0, line=0.0, lat=0.0, z=0.0, noise=0.0, speed=[])
    for k, fp in enumerate(A['fps']):
        u = P[k] @ A['d']
        lat = P[k] @ A['e']
        z = P[k][:, 2]
        for (s, ln) in fp.runs:
            a = s * fp.dt * kappa
            T = ln * fp.dt * kappa
            i0 = int(math.ceil((a + 0.05 * T) / dt))
            i1 = int((a + 0.95 * T) / dt)
            idx = np.arange(i0, i1 + 1) % N
            tt = np.arange(i0, i1 + 1) * dt
            uu = u[idx]
            b, c = np.polyfit(tt, uu, 1)
            res = uu - (b * tt + c)
            worst['slope'] = max(worst['slope'], abs(-b - s_star) / s_star)
            worst['line'] = max(worst['line'], float(np.max(np.abs(res)) / H))
            vel = -(np.roll(u, -1) - u)[idx] / dt
            wl = max(4, int(min(0.08, 0.35 * T) / dt))
            for q in range(0, len(tt) - wl, max(1, wl // 4)):
                bw = np.polyfit(tt[q:q + wl], uu[q:q + wl], 1)[0]
                worst['noise'] = max(worst['noise'], abs(-bw - s_star) / v)
            worst['lat'] = max(worst['lat'], float((lat[idx].max() - lat[idx].min()) / H))
            worst['z'] = max(worst['z'], float((z[idx].max() - z[idx].min()) / H))
            worst['speed'].append(-b)
    sp = sorted(worst['speed'])
    worst['rate'] = v / sp[len(sp) // 2]
    return worst


def make_carry(orig_path, new_path, out_path):
    """CarryWalk is the Walk clip with other arm tracks: rebuild it from the new Walk (all tracks that equal the old Walk)
    and the old CarryWalk arm tracks (time scaled like the new Walk, loop closed). Arguments: skeleton with the old clips,
    skeleton with the new Walk, output."""
    so = Skel(orig_path)
    sn = Skel(new_path)
    ow = so.clips['Walk']
    oc = so.clips['CarryWalk']
    nw = sn.clips['Walk']
    kappa = nw.length / ow.length
    text = open(new_path, encoding='utf-8').read()
    m = re.search(r'<animation name="CarryWalk" length="[^"]+">', text)
    a = m.start()
    e = text.index('</animation>', a) + len('</animation>')
    out = '<animation name="CarryWalk" length="%s">\n\t\t\t<tracks>' % fmt(nw.length)
    for bone in oc.order:
        tr = oc.tracks[bone]
        ot = ow.tracks.get(bone)
        same = (ot is not None and len(ot.t) == len(tr.t) and np.abs(ot.t - tr.t).max() < 1e-6 and
                np.abs(ot.T - tr.T).max() < 2e-5 and np.abs(ot.S - tr.S).max() < 2e-5 and
                min(np.abs(ot.Q - tr.Q).max(), np.abs(ot.Q + tr.Q).max()) < 2e-5)
        if same:
            src = nw.tracks[bone]
            t, T, Q, S = src.t, src.T, src.Q, src.S
        else:
            t, T, Q, S = close_track(tr, kappa, nw.length)
        out += '\n\t\t\t\t<track bone="%s">\n\t\t\t\t\t<keyframes>\n' % bone
        for j in range(len(t)):
            out += key_xml(t[j], T[j], Q[j], S[j], tr.has_scale)
        out += '\t\t\t\t\t</keyframes>\n\t\t\t\t</track>'
    out += '\n\t\t\t</tracks>\n\t\t</animation>'
    new = text[:a] + out + text[e:]
    open(out_path, 'w', encoding='utf-8', newline='\n').write(new)
    print('CarryWalk rebuilt: %d tracks, length %.4f' % (len(oc.order), nw.length))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('cmd', choices=['measure', 'fix', 'carry'])
    ap.add_argument('xml')
    ap.add_argument('rig')
    ap.add_argument('out', nargs='?')
    ap.add_argument('--clip', default='Walk')
    ap.add_argument('--rho', type=float, default=1.0)
    ap.add_argument('--rate', type=float, default=1.0)
    ap.add_argument('--kappa', type=float, default=None)
    ap.add_argument('--search', action='store_true')
    ap.add_argument('--wo', type=float, default=2.0)
    ap.add_argument('--duty', type=float, default=1.0)
    ap.add_argument('--extcap', type=float, default=0.95)
    ap.add_argument('--refine', type=int, default=3)
    ap.add_argument('--maxdrop', type=float, default=None)
    ap.add_argument('--maxerr', type=float, default=0.004)
    ap.add_argument('--keydt', type=float, default=None)
    ap.add_argument('--repair', action='store_true')
    ap.add_argument('--lo', type=float, default=0.5)
    ap.add_argument('--hi', type=float, default=3.5)
    a = ap.parse_args()
    global KEYDT
    if a.keydt:
        KEYDT = a.keydt
    if a.cmd == 'carry':
        make_carry(a.xml, a.rig, a.out)
        return
    rigs = load_rigs()
    cfg = rigs[a.rig]
    skel = Skel(a.xml)
    if a.cmd == 'measure':
        r = measure(skel, cfg, a.clip)
        print(json.dumps(r, default=lambda o: o.tolist() if hasattr(o, 'tolist') else str(o), indent=1))
        return
    if a.search:
        rho, fx = search(skel, cfg, a.clip, a.rate, lo=a.lo, hi=a.hi, maxerr=a.maxerr, wo=a.wo, maxdrop=a.maxdrop, duty=a.duty, extcap=a.extcap, homotopy=a.repair, repair=a.repair)
        print('chosen rho', rho)
        if fx is None:
            print('no feasible stride')
            return
        if a.refine > 0:
            fx = fix_clip(skel, cfg, a.clip, rate=a.rate, rho=rho, wo=a.wo, maxdrop=a.maxdrop, duty=a.duty, extcap=a.extcap, refine=a.refine, homotopy=a.repair, repair=a.repair)
    else:
        fx = fix_clip(skel, cfg, a.clip, rate=a.rate, rho=a.rho, kappa=a.kappa, wo=a.wo, maxdrop=a.maxdrop, duty=a.duty, extcap=a.extcap, refine=a.refine, homotopy=a.repair, repair=a.repair)
    write_xml(a.xml, a.out, skel, a.clip, fx)
    sk2 = Skel(a.out)
    vf = verify_fix(sk2, cfg, fx, a.clip)
    print('verify: rate %.3f slope err %.3f line dev %.4f H wdev %.3f lat %.4f H z %.4f H' % (vf['rate'], vf['slope'], vf['line'], vf['noise'], vf['lat'], vf['z']))
    meta = dict(clip=a.clip, length=fx['Ln'], kappa=fx['kappa'], s_star=fx['s_star'], rate=fx['rate'], v=cfg['v'],
                d=[float(fx['A']['d'][0]), float(fx['A']['d'][1])], H=fx['A']['H'], body=fx['body'],
                dropmax=float(fx['drop'].max()), verify=dict((k, v) for k, v in vf.items() if k != 'speed'),
                feet=[dict(bone=f['units'][0]['chain'][-1], runs=[[s * fp.dt * fx['kappa'], ln * fp.dt * fx['kappa']] for (s, ln) in fp.runs])
                      for f, fp in zip(cfg['feet'], fx['A']['fps'])])
    json.dump(meta, open(a.out + '.json', 'w'))
    print('kappa %.4f length %.4f' % (fx['kappa'], fx['Ln']))
    for s in fx['stats']:
        print('foot %d unit %d  err max %.4f H mean %.4f H' % s)


if __name__ == '__main__':
    main()
