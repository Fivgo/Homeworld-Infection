<!-- done: (none yet) -->
# M3-R2 producer 2: the cutscene's one-frame flicker (key nis-flicker)

Base bbdc613 (this work is `git diff bbdc613` in the m2 worktree). Subsystems: **(a) detect** (the per-frame signal
column, tools/dump_outliers.py, the scans), **(b) cause** (the candidates, each checked), **(c) fix**, **(d) fences**.

Work in progress.
