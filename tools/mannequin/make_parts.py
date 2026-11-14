# make_parts.py [out.h]: cuts the mannequin of actionforge_mannequin_rest_pose.glb (rig by Quaternius, exported by
# ActionForge, CC0 1.0) into rigid parts, one per bone of the humanoid of nbMakeHumanoidBones at 1.8 m (bones.json, written
# by dump_bones.c), and writes them in the frames of those bones as demo/mannequin_parts.h. The model is a set of separate
# shells: each goes whole to the bone it hangs on most, only the shell of head and chest is cut between them. Limbs are
# fitted so that their two joints lie on the joints of the figure, trunk and head keep their shape, the fingers are
# curled, and the small joints of the fingers are left out. Reads the files only, runs nothing from them.
import collections, json, math, os, sys
import numpy as np
M = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, M)
from glb import Glb

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(M, '..', '..', 'demo', 'mannequin_parts.h')
TRUNK = 0.976          # the model is 1.83 m tall, the figure 1.8 m
CURL = (1.25, 1.45, 1.1)   # how far the three joints of a finger are bent, radians
FOOT_HIGH = 0.78       # the foot of the model is taller at the ankle than the foot of the figure
PELVIS, BELLY, RIBS, LOWER_CHEST, CHEST, NECK, HEAD = 0, 1, 2, 3, 4, 5, 6
ARM = {'l': (7, 8, 9), 'r': (10, 11, 12)}
LEG = {'l': (13, 14, 15), 'r': (16, 17, 18)}
FINGERS = ('index', 'middle', 'ring', 'pinky')


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return np.array([aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx, aw * bz + ax * by - ay * bx + az * bw,
                     aw * bw - ax * bx - ay * by - az * bz])


def qmat(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def align(u, v):
    """the smallest rotation that turns the unit vector u into the unit vector v"""
    c = float(np.dot(u, v))
    axis = np.cross(u, v)
    s = float(np.linalg.norm(axis))
    if s < 1e-9:
        return np.eye(3)
    k = axis / s
    K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
    return np.eye(3) + s * K + (1 - c) * (K @ K)


def rot_z(a):
    return np.array([[math.cos(a), -math.sin(a), 0], [math.sin(a), math.cos(a), 0], [0, 0, 1]])


# the figure: world rest pose of its bones
D = json.load(open(os.path.join(M, 'bones.json')))
B = D['bones']
P, Q = [], []
for i, b in enumerate(B):
    if b['parent'] < 0:
        P.append(np.array([0.0, D['rootHeight'], 0.0]) + np.array(b['p']))
        Q.append(np.array(b['q']))
    else:
        P.append(P[b['parent']] + qmat(Q[b['parent']]) @ np.array(b['p']))
        Q.append(qmul(Q[b['parent']], np.array(b['q'])))
R = [qmat(q) for q in Q]

# the model
g = Glb(os.path.join(M, 'actionforge_mannequin_rest_pose.glb'))
names, ibm = g.joints()
J = {n: np.linalg.inv(np.array(m, dtype=float).reshape(4, 4).T)[:3, 3] for n, m in zip(names, ibm)}


def pieces(m):
    pos, nor, jo, we, idx = g.mesh(m)
    key, rep = {}, []
    for i, p in enumerate(pos):
        rep.append(key.setdefault((round(p[0], 4), round(p[1], 4), round(p[2], 4)), i))
    parent = list(range(len(pos)))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x
    for t in range(0, len(idx), 3):
        a = find(rep[idx[t]])
        parent[find(rep[idx[t + 1]])] = a
        parent[find(rep[idx[t + 2]])] = a
    comp = collections.defaultdict(list)
    for t in range(0, len(idx), 3):
        comp[find(rep[idx[t]])].append((idx[t], idx[t + 1], idx[t + 2]))
    out = []
    for tris in comp.values():
        w = collections.Counter()
        for v in {v for t in tris for v in t}:
            for q in range(4):
                if we[v][q] > 0:
                    w[names[jo[v][q]]] += we[v][q]
        out.append((tris, w))
    main = [names[jo[v][max(range(4), key=lambda q: we[v][q])]] for v in range(len(pos))]
    return np.array(pos, dtype=float), np.array(nor, dtype=float), main, out


def side_of(name):
    return name[-1] if name.endswith(('_l', '_r')) else None


def trunk(v):
    hip_m = np.array([0.0, J['thigh_l'][1], 0.0])
    hip_n = np.array([0.0, P[LEG['l'][0]][1], 0.0])
    return hip_n + TRUNK * (v - hip_m), TRUNK * np.eye(3)


def limb(am, bm, an, bn, wide=TRUNK):
    um, un = (bm - am) / np.linalg.norm(bm - am), (bn - an) / np.linalg.norm(bn - an)
    A = align(um, un)
    along = np.linalg.norm(bn - an) / np.linalg.norm(bm - am)
    L = A @ (wide * np.eye(3) + (along - wide) * np.outer(um, um))
    return lambda v: (an + L @ (v - am), L)


def hand(s):
    arm = ARM[s]
    um = np.array([1.0 if s == 'l' else -1.0, 0.0, 0.0])
    un = R[arm[2]] @ np.array([0.0, 1.0, 0.0])
    L = TRUNK * align(um, un)
    return lambda v: (P[arm[2]] + L @ (v - J['hand_' + s]), L)


def foot(s):
    # the foot of the figure stands along the axes of the world in its rest pose, toes ahead: its sole, heel and toes
    # from the corners of its box, and the foot of the model as long and as wide in proportion, lower by FOOT_HIGH
    leg = LEG[s]
    b = B[leg[2]]
    c, h = np.array(b['center']), np.array(b['half'])
    corners = np.array([P[leg[2]] + R[leg[2]] @ (c + h * np.array([i, j, k])) for i in (-1, 1) for j in (-1, 1) for k in (-1, 1)])
    sole, heel, toe = corners[:, 1].min(), corners[:, 2].min(), corners[:, 2].max()
    middle = 0.5 * (corners[:, 0].min() + corners[:, 0].max())
    heel_m, toe_m = FOOT_M[s]
    sz = (toe - heel) / (toe_m - heel_m)
    x0 = J['foot_' + s][0]
    L = np.diag([sz, FOOT_HIGH * sz, sz])

    def f(v):
        return np.array([middle + sz * (v[0] - x0), sole + FOOT_HIGH * sz * v[1], heel + sz * (v[2] - heel_m)]), L
    return f


def curl(s, finger, joint):
    """where a point of the joint-th part of a finger goes as the hand closes, and how it turns"""
    sign = -1.0 if s == 'l' else 1.0
    T, origin = np.eye(3), np.zeros(3)
    chain = []
    for n in range(joint + 1):
        j = J['%s_0%d_%s' % (finger, n + 1, s)]
        chain.append((j, rot_z(sign * CURL[n])))
    def f(v):
        p, L = v.copy(), np.eye(3)
        for j, A in reversed(chain):
            p = j + A @ (p - j)
            L = A @ L
        return p, L
    return f


HIER = ['pelvis', 'spine_01', 'spine_02', 'spine_03', 'neck_01', 'Head', 'clavicle', 'upperarm', 'lowerarm', 'hand', 'thigh', 'calf',
        'foot', 'ball']


def base(name):
    return name[:-2] if side_of(name) else name


def target(name):
    """the bone of the figure a bone of the model belongs to, and the fit"""
    s, b = side_of(name), base(name)
    if b == 'pelvis':
        return PELVIS, trunk
    if b in ('spine_01',):
        return RIBS, trunk
    if b in ('spine_02', 'spine_03', 'clavicle'):
        return CHEST, trunk
    if b == 'neck_01':
        return NECK, trunk
    if b == 'Head':
        return HEAD, trunk
    if b == 'upperarm':
        return ARM[s][0], limb(J['upperarm_' + s], J['lowerarm_' + s], P[ARM[s][0]], P[ARM[s][1]])
    if b == 'lowerarm':
        return ARM[s][1], limb(J['lowerarm_' + s], J['hand_' + s], P[ARM[s][1]], P[ARM[s][2]])
    if b == 'thigh':
        return LEG[s][0], limb(J['thigh_' + s], J['calf_' + s], P[LEG[s][0]], P[LEG[s][1]])
    if b == 'calf':
        return LEG[s][1], limb(J['calf_' + s], J['foot_' + s], P[LEG[s][1]], P[LEG[s][2]])
    if b in ('foot', 'ball'):
        return LEG[s][2], foot(s)
    return ARM[s][2], hand(s)      # the hand and its fingers


parts = [dict(v=[], n=[], m=[], i=[]) for _ in B]


def emit(bone, verts, norms, tris, material, fit, pre=None):
    part = parts[bone]
    index = {}
    for t in tris:
        for v in t:
            if v not in index:
                p, n = verts[v], norms[v]
                L0 = np.eye(3)
                if pre is not None:
                    p, L0 = pre(p)
                w, L = fit(p)
                L = L @ L0
                nn = np.linalg.inv(L).T @ n
                nn = R[bone].T @ (nn / np.linalg.norm(nn))
                index[v] = len(part['v'])
                part['v'].append(R[bone].T @ (w - P[bone]))
                part['n'].append(nn)
                part['m'].append(material)
            part['i'].append(index[v])


FOOT_M = {}
pos0, nor0, main0, pieces0 = pieces(0)
for s in 'lr':
    zs = [pos0[v][2] for tris, w in pieces0 if base(w.most_common(1)[0][0]) in ('foot', 'ball') and side_of(w.most_common(1)[0][0]) == s
          for t in tris for v in t]
    FOOT_M[s] = (min(zs), max(zs))

for mesh, material in ((0, 0), (1, 1)):
    pos, nor, main, ps = pieces(mesh)
    for tris, w in ps:
        top = w.most_common(2)
        name = top[0][0]
        b = base(name)
        if material == 1:
            # a joint goes to the bone above it; the small joints of the fingers are left out
            if any(b.startswith(f) for f in FINGERS + ('thumb',)):
                continue
            other = top[1][0] if len(top) > 1 and top[1][1] > 0.6 * top[0][1] else None
            if other and base(other) in HIER and b in HIER and HIER.index(base(other)) < HIER.index(b) and \
                    side_of(other) in (None, side_of(name)):
                name = other
                b = base(name)
        if material == 0 and b in ('Head', 'spine_03', 'neck_01', 'clavicle') and len(tris) > 500:
            # the shell of head and chest: what hangs on the head alone goes to the head, the rest with the back of the neck
            # to the chest
            head = [t for t in tris if all(main[v] == 'Head' for v in t)]
            chest = [t for t in tris if not all(main[v] == 'Head' for v in t)]
            emit(HEAD, pos, nor, head, material, trunk)
            emit(CHEST, pos, nor, chest, material, trunk)
            continue
        bone, fit = target(name)
        pre = None
        for f in FINGERS:
            if b.startswith(f):
                pre = curl(side_of(name), f, int(b[len(f) + 2]) - 1)
        emit(bone, pos, nor, tris, material, fit, pre)

with open(OUT, 'w') as f:
    f.write('// SPDX-License-Identifier: MIT\n')
    f.write('// Written by tools/mannequin/make_parts.py, do not edit. The mannequin of tools/mannequin/actionforge_mannequin_rest_pose.glb,\n')
    f.write('// rig by Quaternius (quaternius.com), exported by ActionForge (actionforge.app), dedicated to the public domain under CC0 1.0,\n')
    f.write('// cut into one rigid part per bone of the humanoid of nbMakeHumanoidBones at 1.8 m, each in the frame of its bone, in metres.\n')
    f.write('// Material 0 is the body, 1 the joints.\n\n#pragma once\n\n')
    f.write('struct MannequinVertex\n{\n\tfloat position[3];\n\tsigned char normal[3];\n\tunsigned char material;\n};\n\n')
    f.write('struct MannequinPart\n{\n\tint firstVertex;\n\tint vertexCount;\n\tint firstIndex;\n\tint indexCount;\n};\n\n')
    f.write('static const float s_mannequinHeight = 1.8f;\n\n')
    f.write('static const MannequinVertex s_mannequinVertices[] = {\n')
    fv, fi, table = 0, 0, []
    for part in parts:
        for p, n, m in zip(part['v'], part['n'], part['m']):
            c = [int(round(127 * x)) for x in n]
            f.write('\t{ { %.4ff, %.4ff, %.4ff }, { %d, %d, %d }, %d },\n' % (p[0], p[1], p[2], c[0], c[1], c[2], m))
        table.append((fv, len(part['v']), fi, len(part['i'])))
        fv += len(part['v'])
        fi += len(part['i'])
    f.write('};\n\nstatic const unsigned short s_mannequinIndices[] = {\n')
    for part in parts:
        for k in range(0, len(part['i']), 24):
            f.write('\t' + ' '.join('%d,' % x for x in part['i'][k:k + 24]) + '\n')
    f.write('};\n\nstatic const MannequinPart s_mannequinParts[] = {\n')
    for b, t in zip(B, table):
        f.write('\t{ %d, %d, %d, %d }, // %s\n' % (t + (b['name'],)))
    f.write('};\n')
for b, part in zip(B, parts):
    v = np.array(part['v']) if part['v'] else np.zeros((1, 3))
    print('%-16s %5d vertices %5d triangles  x %6.3f..%6.3f  y %6.3f..%6.3f  z %6.3f..%6.3f' % (b['name'], len(part['v']), len(part['i']) // 3,
          v[:, 0].min(), v[:, 0].max(), v[:, 1].min(), v[:, 1].max(), v[:, 2].min(), v[:, 2].max()))
print('in all %d vertices, %d triangles' % (sum(len(p['v']) for p in parts), sum(len(p['i']) for p in parts) // 3))
