#!/usr/bin/env python3
"""Delta counts per M2-R2 stage, for the table in docs/m2-r1-summary.md.

HW1-matched functions: class and Cataclysm bytes from ghidra/delta_map.json, assigned to a stage by
their HW1 source file (STAGE_FILES below); files not listed go by delta-map group (Ships -> stage 2,
other -> stage 3, any other Game file -> stage 2; in practice MEX.c, Trails.c, Titan.c, utility.c).

Cataclysm-only functions (binary-map class cat-new, from tools/binary_map_coverage.py): the kas2c
mission code by address region (binary_map_coverage.MISSION), then by name prefix (PREFIXES), then
the stage of the nearest HW1-matched function in address order. The last rule is a guess: the game
objects link roughly in source-file order, but a Cataclysm-only source file inherits its neighbour's
stage.

Rebuilt in M2-R2 from the integrate step's /tmp/hwi-integrate/stages.py (R1, outside git). Run on
the M2-R1 delta map (git show 13723dd:ghidra/delta_map.json) it reproduces R1's totals: 3,357 / 983 /
80 HW1-matched and 3,789 cat-new functions.

  python3 -I tools/m2_stage_counts.py [--map ghidra/delta_map.json] [--md] [--json OUT]
"""
import argparse
import bisect
import collections
import glob
import importlib.util
import json
import os
import re
import subprocess

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main_root():
    try:
        common = subprocess.check_output(["git", "-C", HERE, "rev-parse", "--path-format=absolute",
                                          "--git-common-dir"], text=True, stderr=subprocess.DEVNULL).strip()
        return os.path.dirname(common)
    except Exception:
        return HERE


EXPORT = os.path.join(main_root(), "ghidra/exports/cataclysm_gog_r6")   # M2-R4: r6
S1, S2, S3, S4, S5, S6 = ("1 formats+data-load", "2 universe+ships", "3 UI+presentation", "4 AI", "5 LAN",
                          "6 missions")
STAGES = [S1, S2, S3, S4, S5, S6]
STAGE_FILES = {
    S1: "Mesh.c MeshAnim.c texreg.c LOD.c StatScript.c SaveGame.c BigFile.c File.c LZSS.c BitIO.c CRC32.c "
        "Tweak.c StringSupport.c BMP.c ObjTypes.c MadLinkIn.c AutoLOD.c",
    S2: "Universe.c UnivUpdate.c Collision.c Physics.c Gun.c Attack.c Damage.c Dock.c LaunchMgr.c Formation.c "
        "FlightMan.c CommandLayer.c CommandWrap.c Clamp.c Blobs.c ResCollect.c Crates.c Tactics.c Captaincy.c "
        "Alliance.c Stats.c ResearchAPI.c Clouds.c Nebulae.c Volume.c Vector.c Matrix.c Randy.c Teams.c "
        "Bounties.c ETG.c Particle.c HS.c Eval.c Transformer.c StubTransform.c Memory.c LinkedList.c Task.c "
        "Globals.c fastmath.c Twiddle.c Undo.c Debug.c Region.c Key.c B-Spline.c Camera.c CameraCommand.c "
        "Select.c ShipSelect.c",
    S3: "FEFlow.c FEReg.c FontReg.c font.c UIControls.c TaskBar.c ConsMgr.c ResearchGUI.c InfoOverlay.c "
        "PiePlate.c Sensors.c Tactical.c ShipView.c ScenPick.c ColPick.c GamePick.c PlugScreen.c Scroller.c "
        "Options.c KeyBindings.c HorseRace.c TradeMgr.c Subtitle.c NIS.c Animatic.c Battle.c SoundEvent.c "
        "SoundEventPlay.c SoundEventStop.c SpeechEvent.c smixer.c soundlow.c sstream.c mixfft.c fqcodec.c "
        "fqeffect.c fquant.c dct.c render.c rinit.c rinit.cpp glcompat.c glcaps.c rglu.c prim2d.c prim3d.c "
        "Light.c Shader.c Clipper.c Star3d.c BTG.c NavLights.c Color.c screenshot.c debugwnd.c mouse.c main.c "
        "mainrgn.c bink.c ddraw.cpp Ping.c Demo.c",
    S4: "AIAttackMan.c AIDefenseMan.c AIEvents.c AIFleetMan.c AIHandler.c AIMoves.c AIOrders.c AIPlayer.c "
        "AIResourceMan.c AIShip.c AITeam.c AITrack.c AIUtilities.c AIVar.c",
    S5: "CommandNetwork.c MultiplayerGame.c MultiplayerLANGame.c TitanNet.c NetCheck.c LagPrint.c ChannelFSM.c "
        "GameChat.c Chatting.c AutoDownloadMap.c TimeoutTimer.c Queue.c Md52.cpp TitanInterface.cpp",
    S6: "KAS.c KASFunc.c Timer.c Objectives.c LevelLoad.c SinglePlayer.c Tutor.c",
}
STAGE_OF = {f.lower(): s for s, fs in STAGE_FILES.items() for f in fs.split()}
PREFIXES = [
    (r"^(cat_)?kas", S6), (r"^cat_(mission|objective|tutor|singlePlayer|sp[A-Z])", S6),
    # M2-R4 rest-oracle: the ship info panel and the selection status bars are UI (docs/m2-r3-summary.md), not the
    # stage-2 neighbours the address rule gave them
    (r"^cat_(shipPrint|selStatus|selHealth)", S3),
    (r"^(cat_)?ai[a-z]?[A-Z_]", S4), (r"^cat_ai", S4),
    (r"^(cat_)?(mg|titan|Titan|lan|LAN|cn[A-Z]|netcheck|chat)", S5),
]
LABEL = {
    S1: "1 file formats + data load", S2: "2 universe and ships", S3: "3 UI and presentation", S4: "4 AI",
    S5: "5 LAN networking", S6: "6 missions",
}


def load_bmc():
    spec = importlib.util.spec_from_file_location("bmc", os.path.join(HERE, "tools/binary_map_coverage.py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def compute(map_path):
    bmc = load_bmc()
    funcs = bmc.load_funcs(EXPORT)
    syms = [os.path.join(HERE, "ghidra/symbols_round%d.json" % i) for i in range(1, 6)]
    props = []
    for r in ("r2", "r3", "r4", "r5"):
        props += sorted(glob.glob(os.path.join(HERE, "ghidra/proposals/%s/*.verified.json" % r)))
    cet = {}
    d = json.load(open(os.path.join(HERE, "ghidra/cet_symbols.json")))
    for s in (d.get("symbols", []) if isinstance(d, dict) else d):
        cet[int(s["address"], 16)] = s
    cls = bmc.build_classes(funcs, syms, props, cet)

    dm = json.load(open(map_path))
    agg = collections.defaultdict(collections.Counter)
    defaulted = collections.Counter()
    stage_at = {}
    for r in dm["functions"]:
        st = STAGE_OF.get((r["hw1_file"] or "").lower())
        if st is None:
            if r["group"] != "Ships":
                defaulted["%s|%s" % (r["hw1_file"], r["group"])] += 1
            st = S3 if r["group"] == "other" else S2
        a = int(r["address"], 16)
        agg[st][r["class"]] += 1
        agg[st][r["class"] + "_b"] += funcs.get(a, {}).get("size", 0)
        stage_at[a] = st
    addrs = sorted(stage_at)

    def nearest(a):
        i = bisect.bisect_left(addrs, a)
        cands = ([addrs[i - 1]] if i > 0 else []) + ([addrs[i]] if i < len(addrs) else [])
        return stage_at[min(cands, key=lambda x: (abs(x - a), x))]

    how = collections.Counter()
    for a, f in sorted(funcs.items()):
        if cls[a][0] != "cat-new":
            continue
        if bmc.MISSION[0] <= a <= bmc.MISSION[1]:
            st, h = S6, "mission region"
        else:
            st = next((s for pat, s in PREFIXES if re.match(pat, f["name"])), None)
            h = "name prefix"
            if st is None:
                st, h = nearest(a), "address neighbour"
        how[h] += 1
        agg[st]["catnew"] += 1
        agg[st]["catnew_b"] += f["size"]
        if h == "mission region":
            agg[st]["mission"] += 1
            agg[st]["mission_b"] += f["size"]
    keys = ("IDENTICAL", "IDENTICAL_b", "MODIFIED", "MODIFIED_b", "REWRITTEN", "REWRITTEN_b", "catnew", "catnew_b",
            "mission", "mission_b")
    out = {s: {k: agg[s][k] for k in keys} for s in STAGES}
    total = {k: sum(out[s][k] for s in STAGES) for k in keys}
    return {"map": os.path.relpath(map_path, HERE), "stages": out, "total": total, "how": dict(how),
            "defaulted": dict(sorted(defaulted.items()))}


def fmt(n):
    return "{:,}".format(n)


def markdown(res, files):
    rows = ["| stage | HW1 files (main ones) | IDENTICAL fn / B | MODIFIED fn / B | REWRITTEN fn / B | "
            "cat-new fn / B (est.) |", "|---|---|---:|---:|---:|---:|"]
    for s in STAGES:
        o = res["stages"][s]
        cn = "%s / %s" % (fmt(o["catnew"]), fmt(o["catnew_b"]))
        if o["mission"]:
            cn += " (%s / %s B are kas2c mission functions)" % (fmt(o["mission"]), fmt(o["mission_b"]))
        rows.append("| %s | %s | %s / %s | %s / %s | %s / %s | %s |" % (
            LABEL[s], files.get(s, ""), fmt(o["IDENTICAL"]), fmt(o["IDENTICAL_b"]), fmt(o["MODIFIED"]),
            fmt(o["MODIFIED_b"]), fmt(o["REWRITTEN"]), fmt(o["REWRITTEN_b"]), cn))
    t = res["total"]
    rows.append("| **total** | | **%s / %s** | **%s / %s** | **%s / %s** | **%s / %s** |" % (
        fmt(t["IDENTICAL"]), fmt(t["IDENTICAL_b"]), fmt(t["MODIFIED"]), fmt(t["MODIFIED_b"]), fmt(t["REWRITTEN"]),
        fmt(t["REWRITTEN_b"]), fmt(t["catnew"]), fmt(t["catnew_b"])))
    return "\n".join(rows)


MAIN_FILES = {
    S1: "Mesh, MeshAnim, texreg, LOD, StatScript, SaveGame, BigFile, File, ObjTypes, StringSupport, Tweak, "
        "LZSS/BitIO/CRC32",
    S2: "Universe, UnivUpdate, CommandLayer, Collision, Physics, Gun, Dock, Formation, ETG, Particle, "
        "Select/ShipSelect, Stats, ResearchAPI, Ships/*",
    S3: "FEFlow/FEReg, UIControls, ConsMgr, ResearchGUI, Options, NIS, sound/speech, render/GL, main/mainrgn, mouse",
    S4: "AI*.c",
    S5: "CommandNetwork, MultiplayerGame, MultiplayerLANGame, TitanNet, NetCheck, ChannelFSM, chat",
    S6: "KAS, KASFunc, Timer, Objectives, LevelLoad, SinglePlayer, Tutor + kas2c mission code",
}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--map", default=os.path.join(HERE, "ghidra/delta_map.json"))
    ap.add_argument("--md", action="store_true", help="print the markdown table")
    ap.add_argument("--json", help="write the counts to this file")
    a = ap.parse_args()
    res = compute(a.map)
    if a.json:
        json.dump(res, open(a.json, "w"), indent=1)
    if a.md:
        print(markdown(res, MAIN_FILES))
    else:
        for s in STAGES:
            print(s, json.dumps(res["stages"][s]))
        print("total", json.dumps(res["total"]))
        print("cat-new assignment", json.dumps(res["how"]), "defaulted HW1 files", json.dumps(res["defaulted"]))


if __name__ == "__main__":
    main()
