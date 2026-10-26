"""Streaming IMA ADPCM in independently decodable, sample-counted DS blocks.

Each block: <hBBH predictor, index, reserved=0, sample_count (1..512),
then low-nibble-first codes for sample_count-1 samples. No padded audio.
"""
import struct

STEPS = (7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,
         50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,
         279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,
         1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,
         3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,
         11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767)
INDEX = (-1,-1,-1,-1,2,4,6,8)


class Encoder:
    def __init__(self):
        self.pending = bytearray()
        self.index = 0

    def feed(self, pcm, final=False):
        self.pending.extend(pcm)
        if final and len(self.pending) % 2:
            raise ValueError("Odd PCM sample length")
        out = bytearray()
        at = 0
        while len(self.pending) - at >= 1024 or (final and at < len(self.pending)):
            size = min(1024, len(self.pending) - at)
            samples = struct.unpack_from(f"<{size // 2}h", self.pending, at)
            at += size
            predictor = samples[0]
            index = self.index
            out.extend(struct.pack("<hBBH", predictor, index, 0, len(samples)))
            packed = 0
            for i, sample in enumerate(samples[1:]):
                step = STEPS[index]
                delta = sample - predictor
                code = 8 if delta < 0 else 0
                delta = abs(delta)
                change = step >> 3
                for bit, value in ((4, step), (2, step >> 1), (1, step >> 2)):
                    if delta >= value:
                        code |= bit
                        delta -= value
                        change += value
                predictor = max(-32768, min(32767, predictor + (-change if code & 8 else change)))
                index = max(0, min(88, index + INDEX[code & 7]))
                if i & 1:
                    out.append(packed | (code << 4))
                else:
                    packed = code
            if (len(samples) - 1) & 1:
                out.append(packed)
            self.index = index
        del self.pending[:at]
        return bytes(out)
