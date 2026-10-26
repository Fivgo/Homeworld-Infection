#!/usr/bin/env python3
"""speech_probe.py - map Cataclysm speech event ids to their phrases, speech-file offsets and subtitle text.

Reads the race's sentence / phrase lookup tables (SoundFX/<race>sentence.lut, <race>phrase.lut, inside cataclysm.big)
the way the engine does (src/Game/SpeechEvent.c SEselectsentence 0x005223c0; src/Game/SoundStructs.h: the Cataclysm
sentence LUT header is 0x10 bytes, one actor per race table) and reads each phrase's "INFO" header (the subtitle text)
from the race's .vce speech file (src/SDL/sstream.c ssSubtitleRead 0x00582610).

  python3 -I tools/formats/speech_probe.py 0x10002169                     # one event (hex ids, any number)
  python3 -I tools/formats/speech_probe.py --mission src/Missions/SinglePlayer/Mission02.c   # every kasfSpeechEvent of a mission
  python3 -I tools/formats/speech_probe.py --grep "faint comm echoes"      # search the subtitle text of every event
  --race beast uses the Beast tables and BeastSpeech.vce. --big / --vce override the default paths.
The actor bits (id & 0x3ff800) pick the subtitle speaker (0x1000 Fleet Command "FC:", 0x2000 the Tactical Officer "TO:",
0x8000 a mission-specific voice); the event number is id & 0x3ff.
"""
import argparse, os, re, struct, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
DEF_BIG = os.path.join(ROOT, 'original', 'scratch-m2', 'm2r2-harness', 'data', 'cataclysm.big')

def big_cat(big, name):
    return subprocess.run([sys.executable, '-I', os.path.join(ROOT, 'tools', 'bigfile.py'), 'cat', big, name],
                          check=True, capture_output=True).stdout

class Tables:
    def __init__(self, big, race, vce):
        sl = big_cat(big, 'soundfx\\%ssentence.lut' % race)
        pl = big_cat(big, 'soundfx\\%sphrase.lut' % race)
        _, self.checksum, self.ncol, self.nev, self.nact, _ = struct.unpack_from('<iIHHHH', sl, 0)
        self.look = struct.unpack_from('<%dh' % (self.ncol * self.nact * self.nev), sl, 0x10)
        _, pck, self.pncol, self.pnsent = struct.unpack_from('<iIHH', pl, 0)
        self.plook = struct.unpack_from('<%di' % (self.pncol * self.pnsent), pl, 0x0C)
        if pck != self.checksum:
            sys.exit('sentence and phrase tables are not from the same generate')
        self.vce = open(vce, 'rb')

    def sentence(self, ev, actor=0):
        base = (actor * self.nev + ev) * self.ncol
        prio, nvar, maxvar, idx = self.look[base:base + 4]
        return prio, nvar, maxvar, idx

    def phrases(self, ev):
        prio, nvar, maxvar, idx = self.sentence(ev)
        out = []
        if nvar == 0:                       # an event with no sentence (SEselectsentence returns SOUND_ERR)
            return (prio, nvar, maxvar, idx), out
        for var in range(nvar):
            for vb in range(max(1, maxvar)):
                ph = idx + var * maxvar + vb
                row = self.plook[ph * self.pncol:(ph + 1) * self.pncol]
                if len(row) < 3 or ph < 0 or ph >= self.pnsent:
                    continue
                dur, noff = row[0], row[2] & 0xff
                out.append((var, vb, dur, [o for o in row[3:3 + noff] if o != -1]))
        return (prio, nvar, maxvar, idx), out

    def subtitle(self, off):
        self.vce.seek(off)
        b = self.vce.read(4096)
        i = b.find(b'INFO')
        if i < 0 or i > 600:
            return '(no INFO header within 600 bytes; tag %r)' % b[:4]
        size = struct.unpack_from('<I', b, i + 4)[0]
        return b[i + 8:i + 8 + size].split(b'\0')[0].decode('latin-1')

def describe(t, v, width):
    ev, actor = v & 0x3ff, v & 0x3ff800
    hdr, ph = t.phrases(ev)
    print('%#010x actor=%#x event=%#x priority=%d variations=%d maxvar=%d phrase=%d' % (v, actor, ev, *hdr))
    for var, vb, dur, offs in ph:
        for o in offs:
            print('    var%d/v%d offset=%d duration=%dms: %s' % (var, vb, o, dur, t.subtitle(o)[:width]))

def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('ids', nargs='*', help='speech event ids (hex)')
    ap.add_argument('--mission', help='a generated mission .c: list every kasfSpeechEvent id in file order')
    ap.add_argument('--grep', help='case-insensitive text to find in every event\'s subtitles')
    ap.add_argument('--race', default='sect', choices=['sect', 'beast'])
    ap.add_argument('--big', default=DEF_BIG)
    ap.add_argument('--vce')
    ap.add_argument('--width', type=int, default=200)
    a = ap.parse_args()
    vce = a.vce or os.path.join(ROOT, 'original', 'game', 'SectSpeech.vce' if a.race == 'sect' else 'BeastSpeech.vce')
    t = Tables(a.big, a.race, vce)
    print('tables: %d events, %d phrases, checksum %#x; speech file %s' % (t.nev, t.pnsent, t.checksum, vce))
    ids = [int(x, 16) for x in a.ids]
    if a.mission:
        for m in re.finditer(r'kasfSpeechEvent\((0x[0-9a-fA-F]+),', open(a.mission).read()):
            v = int(m.group(1), 16)
            if v not in ids:
                ids.append(v)
    if a.grep:
        needle = a.grep.lower()
        for ev in range(t.nev):
            _, ph = t.phrases(ev)
            for var, vb, dur, offs in ph:
                for o in offs:
                    s = t.subtitle(o)
                    if needle in s.lower():
                        print('event=%#x (id low bits) var%d/v%d offset=%d: %s' % (ev, var, vb, o, s[:a.width]))
    for v in ids:
        describe(t, v, a.width)

if __name__ == '__main__':
    main()
