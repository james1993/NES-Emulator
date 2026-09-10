#!/usr/bin/env python3
"""Extract the original game's audio from your own copy of the SWF.

The original's sixteen sound assets are embedded in the Flash file as
DefineSound tags -- fifteen MP3 streams and one ADPCM.  This writes them out
under assets/audio/ named by the cue they belong to, which is the name the
game looks for at runtime.

    python3 tools/extract_audio.py /path/to/Sinjid-Shadow-of-the-Warrior.swf

Nothing produced by this script is redistributable: it is the original
soundtrack, and it stays out of the repository (assets/ is gitignored).  Run
it against a copy you already have.  With no assets present the game falls
back to the synthesised score in src/music.c.

The cue names come from the frame labels of the original's own sound clip, so
the mapping is read out of the file rather than hardcoded.
"""
import os
import struct
import sys

# ---------------------------------------------------------------- SWF reading

def read_swf(path):
    """Return the uncompressed tag body of an FWS/CWS/ZWS file."""
    with open(path, 'rb') as f:
        raw = f.read()
    if len(raw) < 8:
        sys.exit("not a SWF: too short")
    sig = raw[:3]
    if sig == b'FWS':
        return raw[8:]
    if sig == b'CWS':
        import zlib
        return zlib.decompress(raw[8:])
    if sig == b'ZWS':
        try:
            import lzma
        except ImportError:
            sys.exit("this SWF is LZMA-compressed and lzma is unavailable")
        # SWF LZMA: 4-byte compressed length, 5-byte props, then the stream
        props = raw[12:17]
        filt = [{'id': lzma.FILTER_LZMA1,
                 'dict_size': struct.unpack('<I', props[1:5])[0]}]
        return lzma.LZMADecompressor(lzma.FORMAT_RAW, filters=filt).decompress(raw[17:])
    sys.exit("not a SWF (signature %r)" % sig)


def tags(buf, off, end):
    while off + 2 <= end:
        rh, = struct.unpack_from('<H', buf, off)
        off += 2
        code, ln = rh >> 6, rh & 0x3F
        if ln == 0x3F:
            ln, = struct.unpack_from('<I', buf, off)
            off += 4
        yield code, off, ln
        off += ln


def skip_rect(b, o):
    nbits = b[o] >> 3
    return o + ((5 + 4 * nbits) + 7) // 8


# ---------------------------------------------------------------- SWF ADPCM
# One of the sixteen is ADPCM rather than MP3, so it needs decoding to WAV.
# The tables and the block layout are the SWF variant of IMA ADPCM.

STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41,
    45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190,
    209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724,
    796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272,
    2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132,
    7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500,
    20350, 22385, 24623, 27086, 29794, 32767,
]
SWF_INDEX = {
    2: [-1, 2],
    3: [-1, -1, 2, 4],
    4: [-1, -1, -1, -1, 2, 4, 6, 8],
    5: [-1, -1, -1, -1, -1, -1, -1, -1, 1, 2, 4, 6, 8, 10, 13, 16],
}


class Bits:
    def __init__(self, data):
        self.d, self.pos = data, 0

    def read(self, n):
        v = 0
        for _ in range(n):
            byte = self.pos >> 3
            if byte >= len(self.d):
                raise EOFError
            v = (v << 1) | ((self.d[byte] >> (7 - (self.pos & 7))) & 1)
            self.pos += 1
        return v

    def signed(self, n):
        v = self.read(n)
        return v - (1 << n) if v >> (n - 1) else v


def decode_adpcm(data, channels, total):
    """SWF's ADPCM variant.

    It is IMA-like but not IMA: the difference is built by walking the code's
    bits down from k0 rather than by the (2*delta+1)*step/2^shift shortcut,
    and the step index is adjusted from the code with its sign bit *masked
    off* -- which is why the index tables are 2^(bits-1) long and not 2^bits.
    """
    bs = Bits(data)
    nbits = bs.read(2) + 2
    index_table = SWF_INDEX[nbits]
    k0 = 1 << (nbits - 2)
    signmask = 1 << (nbits - 1)
    out = []
    try:
        while len(out) < total * channels:
            pred, idx = [], []
            for _ in range(channels):
                pred.append(bs.signed(16))
                idx.append(bs.read(6))
            out.extend(pred[:channels])
            for _ in range(4095):
                if len(out) >= total * channels:
                    break
                for c in range(channels):
                    delta = bs.read(nbits)
                    step = STEP_TABLE[idx[c]]
                    vpdiff, k = 0, k0
                    while k:
                        if delta & k:
                            vpdiff += step
                        step >>= 1
                        k >>= 1
                    vpdiff += step
                    if delta & signmask:
                        pred[c] -= vpdiff
                    else:
                        pred[c] += vpdiff
                    pred[c] = max(-32768, min(32767, pred[c]))
                    idx[c] = max(0, min(88, idx[c] + index_table[delta & ~signmask]))
                    out.append(pred[c])
    except (EOFError, IndexError):
        pass
    return out


def write_wav(path, samples, rate, channels):
    import wave
    w = wave.open(path, 'wb')
    w.setnchannels(channels)
    w.setsampwidth(2)
    w.setframerate(rate)
    w.writeframes(struct.pack('<%dh' % len(samples),
                              *[max(-32768, min(32767, s)) for s in samples]))
    w.close()


# -------------------------------------------------------------------- main

RATES = {0: 5512, 1: 11025, 2: 22050, 3: 44100}


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    swf = sys.argv[1]
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    outdir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(root, 'assets', 'audio')

    body = read_swf(swf)
    start = skip_rect(body, 0) + 4          # frame rate + frame count

    sounds = {}                            # cid -> (fmt, rate, bits, channels, data)
    labels = {}                            # cue name -> cid

    def walk(off, end, sprite=0):
        frame, label = 1, None
        for code, o, ln in tags(body, off, end):
            if code == 1:
                frame += 1
            elif code == 43 and sprite == 180:
                label = body[o:o + ln].split(b'\0')[0].decode('latin1')
            elif code == 15 and sprite == 180:      # StartSound
                cid, = struct.unpack_from('<H', body, o)
                if label:
                    labels.setdefault(label, cid)
            elif code == 14:                        # DefineSound
                cid, = struct.unpack_from('<H', body, o)
                flags = body[o + 2]
                count, = struct.unpack_from('<I', body, o + 3)
                sounds[cid] = ((flags >> 4) & 0xF, RATES[(flags >> 2) & 3],
                               16 if (flags >> 1) & 1 else 8, (flags & 1) + 1,
                               count, body[o + 7:o + ln])
            elif code == 39:                        # DefineSprite
                sid, _ = struct.unpack_from('<HH', body, o)
                walk(o + 4, o + ln, sid)

    walk(start, len(body))

    if not labels:
        sys.exit("no cue labels found -- is this the right SWF?")

    os.makedirs(outdir, exist_ok=True)
    written = skipped = 0
    for name in sorted(labels):
        cid = labels[name]
        if cid not in sounds:
            print("  %-12s no sound asset" % name)
            skipped += 1
            continue
        fmt, rate, bits, ch, count, data = sounds[cid]
        if fmt == 2:                                # MP3
            # SoundData for MP3 opens with a 2-byte seek-samples field
            path = os.path.join(outdir, name + '.mp3')
            with open(path, 'wb') as f:
                f.write(data[2:])
        elif fmt == 1:                              # ADPCM
            path = os.path.join(outdir, name + '.wav')
            write_wav(path, decode_adpcm(data, ch, count), rate, ch)
        elif fmt in (0, 3):                         # uncompressed
            path = os.path.join(outdir, name + '.wav')
            if bits == 16:
                n = len(data) // 2
                write_wav(path, list(struct.unpack('<%dh' % n, data[:n * 2])), rate, ch)
            else:
                write_wav(path, [(b - 128) << 8 for b in data], rate, ch)
        else:
            print("  %-12s unsupported format %d" % (name, fmt))
            skipped += 1
            continue
        print("  %-12s -> %-16s %6.1f KB" %
              (name, os.path.basename(path), os.path.getsize(path) / 1024))
        written += 1

    print("\n%d written to %s%s" %
          (written, outdir, (", %d skipped" % skipped) if skipped else ""))
    print("These are the original's own recordings.  Keep them local -- they are "
          "not part of this repository and must not be redistributed.")


if __name__ == '__main__':
    main()
