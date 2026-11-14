# make_skin.py [out.h]: fits the mannequin of actionforge_mannequin_rest_pose.glb (rig by Quaternius, exported by
# ActionForge, CC0 1.0) to the humanoid of nbMakeHumanoidBones at 1.8 m (bones.json, written by dump_bones.c) and writes
# it as one skin in the rest pose of that figure, demo/mannequin_skin.h. The model is a set of separate shells. Each is
# fitted with the bone it hangs on most: limbs so that their two joints lie on the joints of the figure, trunk and head
# keep their shape, the chest lies further ahead and the clavicles carry the shoulders onto the shoulders of the figure,
# the fingers are curled and their small joints left out. Every vertex then gets a binding from its weights in the
# model: one bone of the figure, or a pose between two of them, in quarters. The demo moves a vertex with the pose of
# its binding, so the shells bend at the joints as the skin of the model does. A rounded piece in the color of the
# joints on each shoulder closes the notch between the neck and the top of the arm, which the model leaves open. Reads
# the files only, runs nothing from them.
import collections, json, math, os, sys
import numpy as np
M = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, M)
from glb import Glb

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(M, '..', '..', 'demo', 'mannequin_skin.h')
TRUNK = 0.976          # the model is 1.83 m tall, the figure 1.8 m
CURL = (1.25, 1.45, 1.1)   # how far the three joints of a finger are bent, radians
FOOT_HIGH = 0.78       # the foot of the model is taller at the ankle than the foot of the figure
CHEST_AHEAD = 0.03     # the chest of the model lies this far behind the chest of the figure over the same hips, metres
STEPS = 4              # a pose between two bones in this many steps
# the piece that closes the shoulder: its middle from the shoulder joint, inward, up and ahead, and its half axes,
# metres
SHOULDER_PAD = ((-0.025, 0.02, -0.015), (0.065, 0.06, 0.05))
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


def polar(L):
    """a linear map as a turn and a stretch"""
    U, _, Vt = np.linalg.svd(L)
    if np.linalg.det(U @ Vt) < 0.0:
        U[:, -1] = -U[:, -1]
    R = U @ Vt
    return R, R.T @ L


def between(La, Lb, share):
    """the linear map the share from La to Lb, the turn and the stretch each on its own: entry by entry, a map between
    an arm that hangs and one that is stretched out would shrink what it moves"""
    Ra, Sa = polar(La)
    Rb, Sb = polar(Lb)
    rel = Rb @ Ra.T
    angle = math.acos(max(-1.0, min(1.0, 0.5 * (np.trace(rel) - 1.0))))
    R = Ra
    if angle > 1e-9:
        k = np.array([rel[2, 1] - rel[1, 2], rel[0, 2] - rel[2, 0], rel[1, 0] - rel[0, 1]]) / (2.0 * math.sin(angle))
        K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
        R = (np.eye(3) + math.sin(share * angle) * K + (1.0 - math.cos(share * angle)) * (K @ K)) @ Ra
    return R @ ((1.0 - share) * Sa + share * Sb)


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
    skin = [[(names[jo[v][q]], we[v][q]) for q in range(4) if we[v][q] > 0] for v in range(len(pos))]
    return np.array(pos, dtype=float), np.array(nor, dtype=float), main, out, skin


def side_of(name):
    return name[-1] if name.endswith(('_l', '_r')) else None


def trunk(v):
    hip_m = np.array([0.0, J['thigh_l'][1], 0.0])
    hip_n = np.array([0.0, P[LEG['l'][0]][1], 0.0])
    return hip_n + TRUNK * (v - hip_m), TRUNK * np.eye(3)


def ahead(far):
    # the chest of the model, moved ahead onto the chest of the figure
    def f(v):
        w, T = trunk(v)
        return w + np.array([0.0, 0.0, far]), T
    return f


def collar(s):
    # the clavicle of the model on that chest, turned and stretched about its own joint so that the shoulder joint of
    # the model comes to lie on the shoulder joint of the figure
    chest = ahead(CHEST_AHEAD)
    start, _ = chest(J['clavicle_' + s])
    end, _ = chest(J['upperarm_' + s])
    goal = P[ARM[s][0]]
    um, un = (end - start) / np.linalg.norm(end - start), (goal - start) / np.linalg.norm(goal - start)
    along = np.linalg.norm(goal - start) / np.linalg.norm(end - start)
    L = align(um, un) @ (np.eye(3) + (along - 1.0) * np.outer(um, um))

    def f(v):
        w, T = chest(v)
        return start + L @ (w - start), L @ T
    return f


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
        # the top of the upper arm hangs partly on the clavicle, which meets the arm in the shoulder joint, see emit
        fit = limb(J['upperarm_' + s], J['lowerarm_' + s], P[ARM[s][0]], P[ARM[s][1]])
        fit.joint = J['upperarm_' + s]
        return ARM[s][0], fit
    if b == 'lowerarm':
        return ARM[s][1], limb(J['lowerarm_' + s], J['hand_' + s], P[ARM[s][1]], P[ARM[s][2]])
    if b == 'thigh':
        return LEG[s][0], limb(J['thigh_' + s], J['calf_' + s], P[LEG[s][0]], P[LEG[s][1]])
    if b == 'calf':
        return LEG[s][1], limb(J['calf_' + s], J['foot_' + s], P[LEG[s][1]], P[LEG[s][2]])
    if b in ('foot', 'ball'):
        return LEG[s][2], foot(s)
    return ARM[s][2], hand(s)      # the hand and its fingers


def figure_bone(name):
    """the bone of the figure that moves what hangs on a bone of the model"""
    s, b = side_of(name), base(name)
    trunk_bones = {'pelvis': PELVIS, 'spine_01': BELLY, 'spine_02': RIBS, 'spine_03': LOWER_CHEST, 'clavicle': CHEST, 'neck_01': NECK,
                   'Head': HEAD}
    if b in trunk_bones:
        return trunk_bones[b]
    if b in ('upperarm', 'lowerarm'):
        return ARM[s][0 if b == 'upperarm' else 1]
    if b in ('thigh', 'calf'):
        return LEG[s][0 if b == 'thigh' else 1]
    if b in ('foot', 'ball'):
        return LEG[s][2]
    return ARM[s][2]      # the hand and its fingers, which stay a fist


def above(bone):
    """the bones above a bone of the figure, nearest first"""
    out = []
    while B[bone]['parent'] >= 0:
        bone = B[bone]['parent']
        out.append(bone)
    return out


BINDINGS, BOUND = [], {}


def binding(weights):
    """the pose a vertex moves with: the bone it hangs on, or a step between the two it hangs on most, if one is above
    the other in the figure"""
    share = collections.Counter()
    for name, weight in weights:
        share[figure_bone(name)] += weight
    top = share.most_common(2)
    first, second, step = top[0][0], top[0][0], 0
    if len(top) == 2:
        (a, wa), (b, wb) = top
        if a in above(b):
            first, second, step = a, b, int(round(STEPS * wb / (wa + wb)))
        elif b in above(a):
            first, second, step = b, a, int(round(STEPS * wa / (wa + wb)))
        if step == 0:
            second = first
        elif step == STEPS:
            first, step = second, 0
    key = (first, second, step)
    if key not in BOUND:
        # the pose turns about the joint between the two bones, or about the middle of the joints between them
        path = [second] + above(second)
        pivot = np.mean([P[k] for k in path[:path.index(first)]], axis=0) if step else P[first]
        BOUND[key] = len(BINDINGS)
        BINDINGS.append((first, second, step, pivot))
    return BOUND[key]


# the bones of the model that stand otherwise in the figure than their shell is fitted: the chest lies further ahead,
# the neck leads from it to the head, and the clavicles carry the shoulders to the shoulders of the figure
POSED = {'spine_02': ahead(CHEST_AHEAD), 'spine_03': ahead(CHEST_AHEAD), 'neck_01': ahead(0.5 * CHEST_AHEAD), 'clavicle_l': collar('l'),
         'clavicle_r': collar('r')}
VERTS, NORMS, MATS, BINDS, INDEX = [], [], [], [], []


def emit(verts, norms, tris, material, fit, skin, pre=None):
    # every vertex is placed with the bone of its shell, and by its weights in the model with the bones of POSED, as the
    # skin of the model would place it: the top of an upper arm that hangs down stays on the shoulder, the sockets of
    # the chest follow the shoulders, and the neck leads from the chest to the head without a step
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
                shares = [(POSED[name], share) for name, share in skin[v] if name in POSED]
                joint = getattr(fit, 'joint', None)
                if len(shares) == 1 and joint is not None:
                    # the upper arm hangs down, a quarter turn from where the model has it, and its top hangs partly on
                    # the clavicle: it turns down by the share it hangs on the arm, about the shoulder joint, and keeps
                    # its distance from it, so the shoulder stays round and the line from the neck to the shoulder
                    # closed
                    pose, share = shares[0]
                    here, _ = fit(joint)
                    _, Lc = pose(verts[v])
                    L = between(L, Lc, share)
                    w = here + L @ (p - joint)
                elif shares:
                    rest = 1.0 - sum(share for _, share in shares)
                    w, L = rest * w, rest * L
                    for pose, share in shares:
                        wc, Lc = pose(verts[v])
                        w, L = w + share * wc, L + share * Lc
                nn = np.linalg.inv(L).T @ n
                index[v] = len(VERTS)
                VERTS.append(w)
                NORMS.append(nn / np.linalg.norm(nn))
                MATS.append(material)
                BINDS.append(binding(skin[v]))
            INDEX.append(index[v])


FOOT_M = {}
pos0, nor0, main0, pieces0, skin0 = pieces(0)
for s in 'lr':
    zs = [pos0[v][2] for tris, w in pieces0 if base(w.most_common(1)[0][0]) in ('foot', 'ball') and side_of(w.most_common(1)[0][0]) == s
          for t in tris for v in t]
    FOOT_M[s] = (min(zs), max(zs))

for mesh, material in ((0, 0), (1, 1)):
    pos, nor, main, ps, skin = pieces(mesh)
    for tris, w in ps:
        top = w.most_common(2)
        name = top[0][0]
        b = base(name)
        if material == 1:
            # a joint is fitted with the bone above it; the small joints of the fingers are left out
            if any(b.startswith(f) for f in FINGERS + ('thumb',)):
                continue
            other = top[1][0] if len(top) > 1 and top[1][1] > 0.6 * top[0][1] else None
            if other and base(other) in HIER and b in HIER and HIER.index(base(other)) < HIER.index(b) and \
                    side_of(other) in (None, side_of(name)):
                name = other
                b = base(name)
        bone, fit = target(name)
        pre = None
        for f in FINGERS:
            if b.startswith(f):
                pre = curl(side_of(name), f, int(b[len(f) + 2]) - 1)
        emit(pos, nor, tris, material, fit, skin, pre)

# The model has no shell between the chest and the upper arm: the top of the arm lies on the shoulder, and between the
# slope from the neck and the top of the arm a notch stays open, wider where the arm swings or lifts, and what is behind
# the figure shows there. A rounded piece in the color of the joints on each shoulder, on the chest, closes it: it lies
# around the shoulder joint and reaches up under the line from the neck to the top of the arm, as the top of the
# shoulder does on a person, and is hidden under the arm where the arm lies on it.
for s in 'lr':
    side = 1.0 if s == 'l' else -1.0
    centre = P[ARM[s][0]] + np.array([side * SHOULDER_PAD[0][0], SHOULDER_PAD[0][1], SHOULDER_PAD[0][2]])
    radii = np.array(SHOULDER_PAD[1])
    first, rings, around = len(VERTS), 6, 12
    bound = binding([('clavicle_' + s, 1.0)])
    for i in range(rings + 1):
        a = math.pi * i / rings
        for j in range(around):
            b = 2.0 * math.pi * j / around
            n = np.array([math.sin(a) * math.cos(b), math.cos(a), math.sin(a) * math.sin(b)])
            VERTS.append(centre + radii * n)
            NORMS.append((n / radii) / np.linalg.norm(n / radii))
            MATS.append(1)
            BINDS.append(bound)
    for i in range(rings):
        for j in range(around):
            a, b = first + i * around + j, first + i * around + (j + 1) % around
            if i > 0:
                INDEX.extend((a, b, b + around))
            if i + 1 < rings:
                INDEX.extend((a, b + around, a + around))

with open(OUT, 'w') as f:
    f.write('// SPDX-License-Identifier: MIT\n')
    f.write('// Written by tools/mannequin/make_skin.py, do not edit. The mannequin of tools/mannequin/actionforge_mannequin_rest_pose.glb,\n')
    f.write('// rig by Quaternius (quaternius.com), exported by ActionForge (actionforge.app), dedicated to the public domain under CC0 1.0,\n')
    f.write('// fitted to the humanoid of nbMakeHumanoidBones at 1.8 m, as one skin in the rest pose of that figure, in metres, the\n')
    f.write('// figure standing on the origin. Every vertex moves with the pose of its binding. Material 0 is the body, 1 the joints.\n\n')
    f.write('#pragma once\n\n')
    f.write('struct MannequinVertex\n{\n\tfloat position[3];\n\tsigned char normal[3];\n\tunsigned char material;\n'
            '\tunsigned char binding;\n};\n\n')
    f.write('// The rest pose of a bone of the figure, the pose the skin is written in\n')
    f.write('struct MannequinRest\n{\n\tfloat position[3];\n\tfloat rotation[4];\n};\n\n')
    f.write('// A pose a vertex moves with: that of the bone first, or with a share above zero a pose between the bones first and\n'
            '// second, first above second in the figure, turned by that share from one to the other about the pivot\n')
    f.write('struct MannequinBinding\n{\n\tint first;\n\tint second;\n\tfloat share;\n\tfloat pivot[3];\n};\n\n')
    f.write('static const float s_mannequinHeight = 1.8f;\n\n')
    f.write('static const MannequinRest s_mannequinRest[] = {\n')
    for b, p, q in zip(B, P, Q):
        f.write('\t{ { %.6ff, %.6ff, %.6ff }, { %.7ff, %.7ff, %.7ff, %.7ff } }, // %s\n' % (p[0], p[1], p[2], q[0], q[1], q[2], q[3], b['name']))
    f.write('};\n\nstatic const MannequinBinding s_mannequinBindings[] = {\n')
    for first, second, step, pivot in BINDINGS:
        f.write('\t{ %d, %d, %.2ff, { %.6ff, %.6ff, %.6ff } }, // %s%s\n' % (first, second, step / STEPS, pivot[0], pivot[1], pivot[2], B[first]['name'],
                ', %d of %d to %s' % (step, STEPS, B[second]['name']) if step else ''))
    f.write('};\n\nstatic const MannequinVertex s_mannequinVertices[] = {\n')
    for p, n, m, k in zip(VERTS, NORMS, MATS, BINDS):
        c = [int(round(127 * x)) for x in n]
        f.write('\t{ { %.4ff, %.4ff, %.4ff }, { %d, %d, %d }, %d, %d },\n' % (p[0], p[1], p[2], c[0], c[1], c[2], m, k))
    f.write('};\n\nstatic const unsigned short s_mannequinIndices[] = {\n')
    for k in range(0, len(INDEX), 24):
        f.write('\t' + ' '.join('%d,' % x for x in INDEX[k:k + 24]) + '\n')
    f.write('};\n')

# vertices of different shells that lie on each other should move together, or a seam opens as a joint bends
where = collections.defaultdict(set)
for p, k in zip(VERTS, BINDS):
    where[(round(p[0], 3), round(p[1], 3), round(p[2], 3))].add(k)
apart = sum(1 for ks in where.values() if len(ks) > 1)
count = collections.Counter(BINDS)
for k, (first, second, step, pivot) in enumerate(BINDINGS):
    print('%2d %-16s %s %5d vertices' % (k, B[first]['name'], ('%d/%d to %-16s' % (step, STEPS, B[second]['name'])) if step else ' ' * 23, count[k]))
print('in all %d vertices, %d triangles, %d bindings; %d of %d places hold vertices of different bindings' % (
    len(VERTS), len(INDEX) // 3, len(BINDINGS), apart, len(where)))
