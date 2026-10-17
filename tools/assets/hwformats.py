"""Readers for Homeworld: Cataclysm ship assets (stdlib only), shared by export_ships.py.

Every layout here follows the documented formats and their probes:
  * meshes  .geo/.peo : docs/formats/mesh.md (0x44 header, 0x74-byte Cataclysm polygonobject, 0x20 material,
                        16-byte vertex / vertex-info / normal entries, 40-byte polyentry); cross-checked with
                        tools/formats/mesh_probe.py.  HW1's 0x70-byte object layout is also accepted.
  * textures .lif     : docs/formats/textures.md ("Willy 7" v0x104, 0x30 header; paletted = indices + 256 RGBA
                        palette + two 256-byte team-effect tables; Alpha = w*h RGBA + optional w*h team maps);
                        cross-checked with tools/formats/textures_probe.py.
  * LOD scripts .lod  : HW1 reference/HomeworldSDL/src/Game/LOD.c:38-73 (typeN / pMeshFileN keys) and
                        lodMeshFileLoad LOD.c (path = race directory + field).
  * colours           : HW1 Game/Color.h little-endian branch, colRGBA = r | g<<8 | b<<16 | a<<24, i.e. the
                        bytes in memory are R, G, B, A.
"""
import math
import os
import struct
import zlib

# --------------------------------------------------------------------------------------------------------------
# PNG writer (zlib + struct only)
# --------------------------------------------------------------------------------------------------------------

_PNG_SIG = b'\x89PNG\r\n\x1a\n'
_COLOR_TYPE = {'L': (0, 1), 'RGB': (2, 3), 'RGBA': (6, 4)}


def _chunk(tag, data):
    return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)


def png_bytes(width, height, pixels, mode='RGBA', level=6):
    """Encode raw 8-bit pixels (row 0 first, top of the image) as a PNG file. Filter type 0 on every row."""
    ctype, bpp = _COLOR_TYPE[mode]
    stride = width * bpp
    if len(pixels) != stride * height:
        raise ValueError('pixel buffer is %d bytes, expected %d' % (len(pixels), stride * height))
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw += pixels[y * stride:(y + 1) * stride]
    ihdr = struct.pack('>IIBBBBB', width, height, 8, ctype, 0, 0, 0)
    return _PNG_SIG + _chunk(b'IHDR', ihdr) + _chunk(b'IDAT', zlib.compress(bytes(raw), level)) + _chunk(b'IEND', b'')


def png_read(path):
    """Minimal PNG reader for 8-bit L/RGB/RGBA, non-interlaced, all five filter types. Returns (w, h, mode, bytes)."""
    with open(path, 'rb') as fh:
        d = fh.read()
    if d[:8] != _PNG_SIG:
        raise ValueError('not a PNG')
    pos = 8
    idat = bytearray()
    w = h = None
    mode = None
    while pos < len(d):
        n, tag = struct.unpack_from('>I4s', d, pos)
        body = d[pos + 8:pos + 8 + n]
        if tag == b'IHDR':
            w, h, depth, ctype, _c, _f, inter = struct.unpack('>IIBBBBB', body)
            if depth != 8 or inter != 0:
                raise ValueError('unsupported PNG depth %d / interlace %d' % (depth, inter))
            mode = {0: 'L', 2: 'RGB', 6: 'RGBA'}.get(ctype)
            if mode is None:
                raise ValueError('unsupported PNG colour type %d' % ctype)
        elif tag == b'IDAT':
            idat += body
        elif tag == b'IEND':
            break
        pos += 12 + n
    bpp = _COLOR_TYPE[mode][1]
    stride = w * bpp
    raw = zlib.decompress(bytes(idat))
    out = bytearray(stride * h)
    prev = bytearray(stride)
    for y in range(h):
        ft = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        if ft == 1:
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xff
        elif ft == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xff
        elif ft == 3:
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xff
        elif ft == 4:
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xff
        elif ft != 0:
            raise ValueError('bad PNG filter %d' % ft)
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return w, h, mode, bytes(out)


# --------------------------------------------------------------------------------------------------------------
# .lif textures (docs/formats/textures.md)
# --------------------------------------------------------------------------------------------------------------

TRF_Paletted, TRF_Alpha, TRF_TeamColor0, TRF_TeamColor1, TRF_IgnoreDimensions = 0x2, 0x8, 0x10, 0x20, 0x100
_LIF = struct.Struct('<8siiiiIIIIII')   # 0x30


class FormatError(Exception):
    pass


class Lif(object):
    """A decoded .lif. rgba: w*h*4 bytes (R,G,B,A, row 0 = first row stored in the file).
    team0 / team1: w*h bytes (per-pixel team-colour weight, 0..255) or None when the flag is not set."""

    __slots__ = ('path', 'flags', 'width', 'height', 'kind', 'rgba', 'team0', 'team1', 'palette_crc', 'image_crc',
                 'indices', 'palette', 'te0_table', 'te1_table')


def lif_read(path):
    with open(path, 'rb') as fh:
        d = fh.read()
    if len(d) < _LIF.size:
        raise FormatError('short .lif (%d bytes)' % len(d))
    ident, ver, flags, w, h, pcrc, icrc, o_data, o_pal, o_t0, o_t1 = _LIF.unpack_from(d, 0)
    if ident != b'Willy 7\0':
        raise FormatError('ident %r is not "Willy 7"' % ident)
    if ver != 0x104:
        raise FormatError('version 0x%x is not 0x104' % ver)
    if not (0 < w <= 4096 and 0 < h <= 4096):
        raise FormatError('bad size %dx%d' % (w, h))
    lif = Lif()
    lif.path = path
    lif.flags = flags
    lif.width = w
    lif.height = h
    lif.palette_crc = pcrc
    lif.image_crc = icrc
    lif.indices = lif.palette = lif.te0_table = lif.te1_table = None
    n = w * h
    if flags & TRF_Paletted and not flags & TRF_Alpha:
        # [hdr][w*h indices][palette 256 x RGBA][te0 256][te1 256]
        if o_data != 0x30 or o_pal != o_data + n or o_t0 != o_pal + 1024 or o_t1 != o_t0 + 256 \
                or len(d) != o_t1 + 256:
            raise FormatError('paletted block layout does not match the header offsets')
        lif.kind = 'pal8'
        idx = d[o_data:o_data + n]
        pal = d[o_pal:o_pal + 1024]
        lif.indices = idx
        lif.palette = pal
        # expand: palette entries are R,G,B,A bytes (Color.h little-endian colRGBA)
        lut = [pal[4 * i:4 * i + 4] for i in range(256)]
        lif.rgba = b''.join(lut[i] for i in idx)
        t0 = d[o_t0:o_t0 + 256]
        t1 = d[o_t1:o_t1 + 256]
        lif.te0_table = t0
        lif.te1_table = t1
        # team effect tables are per palette index (texreg.c trMeshSortListLoad: teamEffect0[data[index]])
        lif.team0 = idx.translate(t0) if flags & TRF_TeamColor0 else None
        lif.team1 = idx.translate(t1) if flags & TRF_TeamColor1 else None
    else:
        if not flags & TRF_Alpha:
            raise FormatError('neither Paletted nor Alpha flag set (flags 0x%x)' % flags)
        # [hdr][w*h*4 RGBA][te0 w*h]?[te1 w*h]?  (offsets of absent blocks hold left-over values)
        end = 0x30 + 4 * n
        if o_data != 0x30:
            raise FormatError('RGBA data offset 0x%x is not 0x30' % o_data)
        lif.kind = 'rgba'
        lif.rgba = d[0x30:end]
        lif.team0 = lif.team1 = None
        if flags & TRF_TeamColor0:
            if o_t0 != end:
                raise FormatError('team map 0 at 0x%x, expected 0x%x' % (o_t0, end))
            lif.team0 = d[o_t0:o_t0 + n]
            end += n
        if flags & TRF_TeamColor1:
            if o_t1 != end:
                raise FormatError('team map 1 at 0x%x, expected 0x%x' % (o_t1, end))
            lif.team1 = d[o_t1:o_t1 + n]
            end += n
        if end != len(d):
            raise FormatError('RGBA blocks end at 0x%x, file is 0x%x' % (end, len(d)))
    if len(lif.rgba) != 4 * n:
        raise FormatError('decoded %d bytes, expected %d' % (len(lif.rgba), 4 * n))
    return lif


def has_alpha(rgba):
    """True when any pixel's alpha byte is not 255."""
    a = rgba[3::4]
    return a.count(255) != len(a)


# --------------------------------------------------------------------------------------------------------------
# .geo / .peo meshes (docs/formats/mesh.md)
# --------------------------------------------------------------------------------------------------------------

_GEOHDR = struct.Struct('<8sIIIIIIIII24s')   # 0x44
GEO_HDR = 0x44
OBJ_CAT, OBJ_HW1 = 0x74, 0x70
MAT_SIZE, VERT_SIZE, NORM_SIZE, POLY_SIZE = 0x20, 0x10, 0x10, 0x28

# material flags, HW1 Game/Mesh.h:62-68
MDF_Smoothing, MDF_2Sided, MDF_BaseColor, MDF_StripeColor, MDF_SelfIllum = 0x2, 0x8, 0x10, 0x20, 0x40


class MeshObject(object):
    __slots__ = ('index', 'name', 'flags', 'n_face_normals', 'n_vertex_normals', 'verts', 'vinfo', 'normals',
                 'polys', 'mother', 'daughter', 'sister', 'matrix')


class Material(object):
    __slots__ = ('index', 'name', 'ambient', 'diffuse', 'specular', 'k_alpha', 'texture', 'flags')


class Mesh(object):
    __slots__ = ('path', 'ident', 'version', 'layout', 'objects', 'materials')


def _cstr(d, off):
    end = d.index(b'\0', off)
    return d[off:end].decode('latin-1')


def mesh_read(path):
    with open(path, 'rb') as fh:
        d = fh.read()
    if len(d) < GEO_HDR:
        raise FormatError('short mesh file (%d bytes)' % len(d))
    ident, version, _oname, file_size, _local, n_pub, n_loc, o_pub, o_loc, n_obj, _res = _GEOHDR.unpack_from(d, 0)
    ident = ident.rstrip(b'\0').decode('latin-1')
    if ident not in ('RMF97ba', 'RMF99ba'):
        raise FormatError('ident %r is not RMF97ba/RMF99ba' % ident)
    if o_pub == GEO_HDR + n_obj * OBJ_CAT:
        stride, layout = OBJ_CAT, 'cataclysm-0x74'
    elif o_pub == GEO_HDR + n_obj * OBJ_HW1:
        stride, layout = OBJ_HW1, 'hw1-0x70'
    else:
        raise FormatError('material table at 0x%x fits neither the 0x74 (Cataclysm) nor the 0x70 (HW1) object '
                          'stride for %d objects' % (o_pub, n_obj))
    mesh = Mesh()
    mesh.path = path
    mesh.ident = ident
    mesh.version = version
    mesh.layout = layout
    mats = []
    for i in range(n_pub + n_loc):
        o = o_pub + i * MAT_SIZE
        o_name, amb, dif, spec, k_alpha, o_tex, flags = struct.unpack_from('<IIIIfIH', d, o)
        m = Material()
        m.index = i
        m.name = _cstr(d, o_name) if o_name else ''
        m.ambient, m.diffuse, m.specular = amb, dif, spec
        m.k_alpha = k_alpha
        m.texture = _cstr(d, o_tex) if o_tex else None
        m.flags = flags
        mats.append(m)
    mesh.materials = mats
    objs = []
    for i in range(n_obj):
        o = GEO_HDR + i * stride
        if stride == OBJ_CAT:
            (o_name, flags, _iobj, _crc, n_v, n_fn, n_vn, n_p,
             p_v, p_vi, p_n, p_p, p_mo, p_da, p_si) = struct.unpack_from('<IBBHiiiiIIIIIII', d, o)
            m_off = o + 0x34
        else:
            (o_name, flags, _iobj, _crc, n_v, n_fn, n_vn, n_p,
             p_v, p_n, p_p, p_mo, p_da, p_si) = struct.unpack_from('<IBBHiiiiIIIIII', d, o)
            p_vi = None
            m_off = o + 0x30
        for lab, ptr, cnt, sz in (('vertex', p_v, n_v, VERT_SIZE), ('normal', p_n, n_fn + n_vn, NORM_SIZE),
                                  ('polygon', p_p, n_p, POLY_SIZE)):
            if cnt < 0 or ptr + cnt * sz > len(d):
                raise FormatError('object %d %s list 0x%x + %d x %d runs past the end of the file'
                                  % (i, lab, ptr, cnt, sz))
        ob = MeshObject()
        ob.index = i
        ob.name = _cstr(d, o_name) if o_name else ''
        ob.flags = flags
        ob.n_face_normals = n_fn
        ob.n_vertex_normals = n_vn
        ob.verts = [struct.unpack_from('<fffi', d, p_v + k * VERT_SIZE) for k in range(n_v)]
        ob.vinfo = ([struct.unpack_from('<II', d, p_vi + k * 16) for k in range(n_v)] if p_vi is not None else None)
        ob.normals = [struct.unpack_from('<fff', d, p_n + k * NORM_SIZE) for k in range(n_fn + n_vn)]
        polys = []
        for k in range(n_p):
            q = p_p + k * POLY_SIZE
            i_fn, i0, i1, i2, i_mat = struct.unpack_from('<iHHHH', d, q)
            uv = struct.unpack_from('<6f', d, q + 12)
            pflags, r0, r1 = struct.unpack_from('<HBB', d, q + 36)
            polys.append((i_fn, i0, i1, i2, i_mat, uv, pflags, r0, r1))
        ob.polys = polys

        def link(ptr):
            if not ptr:
                return None
            if (ptr - GEO_HDR) % stride or not 0 <= (ptr - GEO_HDR) // stride < n_obj:
                raise FormatError('object %d hierarchy link 0x%x is not an object boundary' % (i, ptr))
            return (ptr - GEO_HDR) // stride
        ob.mother, ob.daughter, ob.sister = link(p_mo), link(p_da), link(p_si)
        ob.matrix = struct.unpack_from('<16f', d, m_off)    # column-major hmatrix (glMultMatrixf order)
        # index sanity (docs/m2-r2-mesh.md: every index of every shipped object is in range)
        nn = n_fn + n_vn
        for (i_fn, i0, i1, i2, i_mat, _uv, _pf, _r0, _r1) in polys:
            if not (0 <= i_fn < nn and i0 < n_v and i1 < n_v and i2 < n_v and i_mat < len(mats)):
                raise FormatError('object %d has a polygon index out of range' % i)
        for (_x, _y, _z, ivn) in ob.verts:
            if not 0 <= ivn < nn:
                raise FormatError('object %d has a vertex-normal index out of range' % i)
        objs.append(ob)
    mesh.objects = objs
    return mesh


def mat_mul(a, b):
    """Column-major 4x4 product a*b (both 16-tuples, element [col*4+row])."""
    out = [0.0] * 16
    for c in range(4):
        for r in range(4):
            out[c * 4 + r] = (a[0 * 4 + r] * b[c * 4 + 0] + a[1 * 4 + r] * b[c * 4 + 1] +
                              a[2 * 4 + r] * b[c * 4 + 2] + a[3 * 4 + r] * b[c * 4 + 3])
    return tuple(out)


def world_matrices(mesh):
    """World matrix per object: product of the mother chain's localMatrix, as meshObjectRenderR
    (HW1 Game/Mesh.c, glMultMatrixf(localMatrix) per level, daughters inside their mother) composes them."""
    out = [None] * len(mesh.objects)

    def get(i, depth=0):
        if out[i] is not None:
            return out[i]
        ob = mesh.objects[i]
        if ob.mother is None or depth > 64:
            out[i] = ob.matrix
        else:
            out[i] = mat_mul(get(ob.mother, depth + 1), ob.matrix)
        return out[i]
    for i in range(len(mesh.objects)):
        get(i)
    return out


def reachable_objects(mesh):
    """Objects meshRender draws: object 0 and its sister chain, recursively their daughters (HW1 meshRender)."""
    seen = set()
    stack = []
    if mesh.objects:
        o = 0
        while o is not None and o not in seen:
            stack.append(o)
            seen.add(o)
            o = mesh.objects[o].sister
    while stack:
        i = stack.pop()
        d = mesh.objects[i].daughter
        while d is not None and d not in seen:
            seen.add(d)
            stack.append(d)
            d = mesh.objects[d].sister
    return seen


def xform_point(m, x, y, z):
    return (m[0] * x + m[4] * y + m[8] * z + m[12],
            m[1] * x + m[5] * y + m[9] * z + m[13],
            m[2] * x + m[6] * y + m[10] * z + m[14])


def xform_normal(m, x, y, z):
    nx = m[0] * x + m[4] * y + m[8] * z
    ny = m[1] * x + m[5] * y + m[9] * z
    nz = m[2] * x + m[6] * y + m[10] * z
    ln = math.sqrt(nx * nx + ny * ny + nz * nz)
    if ln < 1e-12:
        return (0.0, 0.0, 1.0)
    return (nx / ln, ny / ln, nz / ln)


def color_rgb(c):
    """materialentry colour -> (r, g, b) floats; bytes in memory are R, G, B, A (Color.h little-endian)."""
    return ((c & 0xff) / 255.0, ((c >> 8) & 0xff) / 255.0, ((c >> 16) & 0xff) / 255.0)


# --------------------------------------------------------------------------------------------------------------
# .lod scripts
# --------------------------------------------------------------------------------------------------------------

def lod_read(path):
    """Return {level: {'type': str, 'mesh': str or None}} from a .lod script (LOD.c:38-73 keys)."""
    levels = {}
    with open(path, 'rb') as fh:
        text = fh.read().decode('latin-1')
    for line in text.splitlines():
        line = line.split(';')[0].strip()
        if not line or line.startswith('['):
            continue
        parts = line.split(None, 1)
        if len(parts) != 2:
            continue
        key, val = parts[0].lower(), parts[1].strip()
        if key[:4] == 'type' and key[4:].isdigit():
            levels.setdefault(int(key[4:]), {}).update(type=val.upper())
        elif key[:9] == 'pmeshfile' and key[9:].isdigit():
            levels.setdefault(int(key[9:]), {}).update(mesh=val)
    return levels


class DataTree(object):
    """Case-insensitive view of the extracted archives: update.big first, then cataclysm.big
    (docs/big-archive-contents.md section 1.7)."""

    def __init__(self, data_root, update_root=None):
        self.data_root = data_root
        self.update_root = update_root
        self.index = {}
        self.overrides = 0
        self.update_keys = set()
        self.used = set()
        for root, is_update in ((data_root, False), (update_root, True)):
            if not root or not os.path.isdir(root):
                continue
            for dp, _dn, fn in os.walk(root):
                for f in fn:
                    full = os.path.join(dp, f)
                    key = os.path.relpath(full, root).replace(os.sep, '/').lower()
                    if is_update and key in self.index:
                        self.overrides += 1
                    if is_update:
                        self.update_keys.add(key)
                    if is_update or key not in self.index:
                        self.index[key] = full

    def resolve(self, rel):
        key = rel.replace('\\', '/').lower().lstrip('/')
        path = self.index.get(key)
        if path is not None:
            self.used.add(key)
        return path

    def used_from_update(self):
        return sorted(self.used & self.update_keys)

    def listdir(self, rel_dir):
        pre = rel_dir.replace('\\', '/').lower().rstrip('/') + '/'
        return sorted(k for k in self.index if k.startswith(pre) and '/' not in k[len(pre):])

    def walk(self, rel_dir):
        pre = rel_dir.replace('\\', '/').lower().rstrip('/') + '/'
        return sorted(k for k in self.index if k.startswith(pre))
