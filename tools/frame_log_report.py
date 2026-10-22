#!/usr/bin/env python3
"""frame_log_report.py - summarise a /frameLog file (HWI-TEST, M3-R2 instrumentation; docs/m3-r2-instrumentation.md,
docs/features/high-refresh-4k.md "The soak").

Usage: python3 -I tools/frame_log_report.py <frame.log> [--window MIN] [--tolerance FRAC] [--oneline]

The log (src/Game/Instrument.c): a header ("# hwi-framelog v1 ... refresh=R (...)", "# settings: <the settings line>"),
then per second one "s=N t=.. frames=.. ft_p50=.. ... " line and one "h s=N bin:count,..." line (the second's frame
periods in 0.1 ms bins; bin 1000 = 100 ms or more).

Prints:
  the whole run: seconds, frames, the frame period's p50 / p95 / p99 / max (from the histograms: 0.1 ms resolution,
      the bin's upper edge), the frames over one refresh interval (and by 1 / 2 / 3+ refreshes), the universe ticks
      (a second's count; the per-tick cost's median of the per-second medians and the largest max), the vertices drawn;
  the drift test (the M3 soak's "no drift"): the first and the last --window minutes (default 10; a run shorter than
      twice that compares its first and last thirds), each one's p50 / p99 from the histograms; PASS when the last
      window's p50 and p99 are each within --tolerance (default 0.10 = 10%) of the first's (or no larger);
  the heap: the first and last heap_used, the growth a minute (least squares over the seconds), the growth heaps made,
      the 'dupe trcolorinfo list' blocks first / last (-1: the release build keeps no names);
  the seconds over budget: the seconds whose ft_p99 exceeds the refresh interval, and how many.
--oneline prints one summary line (the harness's summary).
"""
import sys


def parse(path):
    header = {}
    settings = ""
    secs = []
    hists = {}
    with open(path, "r", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("# hwi-framelog"):
                for tok in line.split():
                    if "=" in tok:
                        k, v = tok.split("=", 1)
                        header[k] = v
            elif line.startswith("# settings:"):
                settings = line[len("# settings:"):].strip()
            elif line.startswith("s="):
                rec = {}
                for tok in line.split():
                    if "=" in tok:
                        k, v = tok.split("=", 1)
                        try:
                            rec[k] = float(v)
                        except ValueError:
                            rec[k] = v
                secs.append(rec)
            elif line.startswith("h s="):
                parts = line.split()
                s = int(parts[1][2:])
                h = {}
                if len(parts) > 2:
                    for item in parts[2].split(","):
                        b, c = item.split(":")
                        h[int(b)] = h.get(int(b), 0) + int(c)
                hists[s] = h
    return header, settings, secs, hists


def hist_quantile(h, q):
    total = sum(h.values())
    if total == 0:
        return 0.0
    rank = max(1, -(-total * q // 100))      # nearest rank, ceil
    acc = 0
    for b in sorted(h):
        acc += h[b]
        if acc >= rank:
            return (b + 1) / 10.0             # the bin's upper edge, ms
    return (max(h) + 1) / 10.0


def merge(hists, seconds):
    out = {}
    for s in seconds:
        for b, c in hists.get(s, {}).items():
            out[b] = out.get(b, 0) + c
    return out


def median(values):
    v = sorted(values)
    return v[len(v) // 2] if v else 0.0


def main(argv):
    if not argv or argv[0].startswith("-"):
        print(__doc__)
        return 2
    path = argv[0]
    window = 10.0
    tol = 0.10
    oneline = False
    i = 1
    while i < len(argv):
        if argv[i] == "--window":
            window = float(argv[i + 1]); i += 1
        elif argv[i] == "--tolerance":
            tol = float(argv[i + 1]); i += 1
        elif argv[i] == "--oneline":
            oneline = True
        else:
            print("frame_log_report: unknown argument %s" % argv[i], file=sys.stderr)
            return 2
        i += 1
    header, settings, secs, hists = parse(path)
    if not secs:
        print("frame_log_report: no seconds in %s" % path)
        return 1
    refresh = float(header.get("refresh", "144"))
    interval = 1000.0 / refresh
    nums = [int(r["s"]) for r in secs]
    allh = merge(hists, nums)
    frames = int(sum(r.get("frames", 0) for r in secs))
    over = int(sum(r.get("over", 0) for r in secs))
    over1 = int(sum(r.get("over1", 0) for r in secs))
    over2 = int(sum(r.get("over2", 0) for r in secs))
    over3 = int(sum(r.get("over3p", 0) for r in secs))
    ticks = int(sum(r.get("ticks", 0) for r in secs))
    dur = secs[-1].get("t", float(len(secs)))
    tick_p50 = median([r.get("tick_p50", 0.0) for r in secs if r.get("ticks", 0) > 0])
    tick_max = max([r.get("tick_max", 0.0) for r in secs] or [0.0])
    verts = [r.get("verts_p50", 0.0) for r in secs if r.get("verts_max", 0) > 0]
    verts_max = max([r.get("verts_max", 0.0) for r in secs] or [0.0])
    # drift: the first and last windows (seconds by t)
    wsec = window * 60.0
    if dur >= 2 * wsec:
        first = [int(r["s"]) for r in secs if r["t"] <= wsec]
        last = [int(r["s"]) for r in secs if r["t"] > dur - wsec]
        wname = "%g min" % window
    else:
        third = max(1, len(secs) // 3)
        first = nums[:third]
        last = nums[-third:]
        wname = "first / last third (%d s each; the run is shorter than 2 x %g min)" % (third, window)
    hf, hl = merge(hists, first), merge(hists, last)
    f50, f99, l50, l99 = hist_quantile(hf, 50), hist_quantile(hf, 99), hist_quantile(hl, 50), hist_quantile(hl, 99)
    drift_ok = (l50 <= f50 * (1 + tol) + 0.1) and (l99 <= f99 * (1 + tol) + 0.1)
    # heap
    heap = [(r["t"], r.get("heap_used", 0.0)) for r in secs]
    if len(heap) >= 2:
        n = len(heap)
        mt = sum(t for t, _ in heap) / n
        mh = sum(h for _, h in heap) / n
        den = sum((t - mt) ** 2 for t, _ in heap)
        slope = (sum((t - mt) * (h - mh) for t, h in heap) / den * 60.0) if den > 0 else 0.0
    else:
        slope = 0.0
    pools = int(secs[-1].get("growth_pools", 0))
    dupe0, dupe1 = int(secs[0].get("dupe", -1)), int(secs[-1].get("dupe", -1))
    budget = [int(r["s"]) for r in secs if r.get("ft_p99", 0.0) > interval]
    p50, p95, p99 = hist_quantile(allh, 50), hist_quantile(allh, 95), hist_quantile(allh, 99)
    fmax = max([r.get("ft_max", 0.0) for r in secs] or [0.0])
    if oneline:
        print("seconds=%d frames=%d refresh=%g ft p50=%.1f p95=%.1f p99=%.1f max=%.3f over=%d (%.1f%%) ticks=%d tick_p50=%.3f "
              "tick_max=%.3f heap %d->%d (%+.0f B/min) pools=%d dupe=%d->%d drift(%s)=%s seconds_over_budget=%d"
              % (len(secs), frames, refresh, p50, p95, p99, fmax, over, 100.0 * over / max(frames, 1), ticks, tick_p50,
                 tick_max, int(heap[0][1]), int(heap[-1][1]), slope, pools, dupe0, dupe1,
                 "%gmin" % window if dur >= 2 * wsec else "thirds", "PASS" if drift_ok else "FAIL", len(budget)))
        return 0
    print("frame log: %s" % path)
    print("header: %s" % " ".join("%s=%s" % kv for kv in header.items()))
    print("settings: %s" % settings)
    print("whole run: %d seconds (t=%.1f), %d frames (%.1f a second)" % (len(secs), dur, frames, frames / max(dur, 1e-9)))
    print("  frame period (ms, 0.1 ms bins, upper edge): p50 %.1f  p95 %.1f  p99 %.1f  max %.3f  (refresh %g Hz: interval %.3f ms)"
          % (p50, p95, p99, fmax, refresh, interval))
    print("  frames over one refresh interval: %d (%.2f%%): 1 refresh late %d, 2 late %d, 3+ late %d"
          % (over, 100.0 * over / max(frames, 1), over1, over2, over3))
    print("  universe ticks: %d (%.2f a second); a tick's cost: median of the per-second medians %.3f ms, largest %.3f ms"
          % (ticks, ticks / max(dur, 1e-9), tick_p50, tick_max))
    print("  vertices drawn: median of the per-second medians %.0f, largest %.0f" % (median(verts), verts_max))
    print("drift (%s): first p50 %.1f p99 %.1f, last p50 %.1f p99 %.1f, tolerance %.0f%%: %s"
          % (wname, f50, f99, l50, l99, tol * 100, "PASS" if drift_ok else "FAIL"))
    print("heap: used %d -> %d bytes (growth %+d), %+.0f bytes a minute (least squares), growth heaps %d, "
          "'dupe trcolorinfo list' blocks %d -> %d"
          % (int(heap[0][1]), int(heap[-1][1]), int(heap[-1][1] - heap[0][1]), slope, pools, dupe0, dupe1))
    print("seconds over budget (ft_p99 > %.3f ms): %d%s" % (interval, len(budget),
          (": " + ",".join(str(s) for s in budget[:20]) + (" ..." if len(budget) > 20 else "")) if budget else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
