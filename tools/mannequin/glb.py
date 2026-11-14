# glb.py: read a binary glTF without running anything from it: json chunk, accessors as tuples, joint names and origins
import json, struct
FMT = {5120: 'b', 5121: 'B', 5122: 'h', 5123: 'H', 5125: 'I', 5126: 'f'}
N = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


class Glb:
    def __init__(self, path):
        b = open(path, 'rb').read()
        assert b[:4] == b'glTF'
        n, t = struct.unpack_from('<II', b, 12)
        self.j = json.loads(b[20:20 + n].decode('utf-8'))
        self.bin = b[20 + n + 8:]

    def read(self, i):
        a = self.j['accessors'][i]
        v = self.j['bufferViews'][a['bufferView']]
        comps = N[a['type']]
        fmt = '<' + FMT[a['componentType']] * comps
        size = struct.calcsize(fmt)
        stride = v.get('byteStride', size)
        off = v.get('byteOffset', 0) + a.get('byteOffset', 0)
        return [struct.unpack_from(fmt, self.bin, off + k * stride) for k in range(a['count'])]

    def mesh(self, m):
        p = self.j['meshes'][m]['primitives'][0]
        at = p['attributes']
        pos = self.read(at['POSITION'])
        nor = self.read(at['NORMAL'])
        jo = self.read(at['JOINTS_0'])
        we = self.read(at['WEIGHTS_0'])
        idx = [x[0] for x in self.read(p['indices'])]
        return pos, nor, jo, we, idx

    def joints(self, skin=0):
        s = self.j['skins'][skin]
        names = [self.j['nodes'][n].get('name', '?') for n in s['joints']]
        ibm = self.read(s['inverseBindMatrices'])
        return names, ibm
