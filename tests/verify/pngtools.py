"""PNG（8bit RGB）の読み書きと、画像の比較・一覧画像（コンタクトシート）作り。標準ライブラリだけで動く。"""

import struct
import zlib


def read_png(path):
    """(幅, 高さ, RGB の bytearray) を返す。stb_image_write が書く 8bit RGB の PNG を想定"""
    data = open(path, 'rb').read()
    pos, idat, w, h = 8, b'', 0, 0
    while pos < len(data):
        n, = struct.unpack('>I', data[pos:pos + 4])
        tag, body = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + n]
        if tag == b'IHDR':
            w, h = struct.unpack('>II', body[:8])
            if body[8] != 8 or body[9] != 2:
                raise ValueError(f'{path}: only 8bit RGB PNG is supported')
        elif tag == b'IDAT':
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    bpp, stride = 3, w * 3
    out = bytearray(h * stride)
    prev = bytearray(stride)
    i = 0
    for y in range(h):
        f = raw[i]
        line = bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        if f == 1:
            for x in range(bpp, stride):
                line[x] = (line[x] + line[x - bpp]) & 255
        elif f == 2:
            for x in range(stride):
                line[x] = (line[x] + prev[x]) & 255
        elif f == 3:
            for x in range(stride):
                a = line[x - bpp] if x >= bpp else 0
                line[x] = (line[x] + (a + prev[x]) // 2) & 255
        elif f == 4:
            for x in range(stride):
                a = line[x - bpp] if x >= bpp else 0
                b, c = prev[x], (prev[x - bpp] if x >= bpp else 0)
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return w, h, out


def write_png(path, w, h, rgb):
    raw = b''.join(b'\x00' + bytes(rgb[y * w * 3:(y + 1) * w * 3]) for y in range(h))

    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
                chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


def frame_diff(a, b, threshold=24):
    """2 枚の差: (平均の差, はっきり変わった画素の割合 [%])"""
    diff = [abs(x - y) for x, y in zip(a, b)]
    changed = sum(1 for j in range(0, len(diff), 3) if max(diff[j], diff[j + 1], diff[j + 2]) > threshold)
    return sum(diff) / len(diff), 100.0 * changed / (len(diff) // 3)


def mean_brightness(rgb):
    return sum(rgb) / len(rgb)


def downscale2(w, h, rgb):
    """幅・高さを半分に（2x2 の平均）"""
    w2, h2 = w // 2, h // 2
    out = bytearray(w2 * h2 * 3)
    for y in range(h2):
        r0, r1 = (2 * y) * w * 3, (2 * y + 1) * w * 3
        for x in range(w2):
            for c in range(3):
                i = 2 * x * 3 + c
                out[(y * w2 + x) * 3 + c] = (rgb[r0 + i] + rgb[r0 + i + 3] + rgb[r1 + i] + rgb[r1 + i + 3]) // 4
    return w2, h2, out


def contact_sheet(path, images, cols):
    """画像を cols 列に並べた 1 枚を作る（幅 1000px を超える画像は半分に縮小）"""
    frames = [read_png(p) for p in images]
    if frames[0][0] > 1000:
        frames = [downscale2(*f) for f in frames]
    w, h = frames[0][0], frames[0][1]
    rows = (len(frames) + cols - 1) // cols
    out = bytearray(w * cols * h * rows * 3)
    for k, (fw, fh, px) in enumerate(frames):
        ox, oy = (k % cols) * w, (k // cols) * h
        for y in range(min(h, fh)):
            dst = ((oy + y) * w * cols + ox) * 3
            out[dst:dst + min(w, fw) * 3] = px[y * fw * 3:y * fw * 3 + min(w, fw) * 3]
    write_png(path, w * cols, h * rows, out)
