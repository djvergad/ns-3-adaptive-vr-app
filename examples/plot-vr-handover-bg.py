#!/usr/bin/env python3
"""Plot the output of vr-nr-oran-handover-bg for runs with and without handover.

Usage: plot-vr-handover-bg.py <prefix-with-handover> <prefix-without-handover> [output.png]
"""
import csv
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

with_ho, without_ho = sys.argv[1], sys.argv[2]
out = sys.argv[3] if len(sys.argv) > 3 else "vr-handover-bg-comparison.png"


def read(name):
    """Read a CSV file into a list of dicts."""
    with open(name) as f:
        return list(csv.DictReader(f))


def series(rows, ue, col):
    """Return (times, values) of a column for one UE."""
    r = [x for x in rows if int(x["UeId"]) == ue]
    return [float(x["Time_s"]) for x in r], [float(x[col]) for x in r]


runs = {"with handover": read(f"{with_ho}-timeseries.csv"), "without handover": read(f"{without_ho}-timeseries.csv")}
gnbs = read(f"{with_ho}-gnb-positions.csv")
ues = sorted({int(r["UeId"]) for r in runs["with handover"]})
static_ues = [u for u in ues if u != 0]

fig, axs = plt.subplots(5, 2, figsize=(15, 17))

# Locations
ax = axs[0][0]
ax.scatter([float(g["X_m"]) for g in gnbs], [float(g["Y_m"]) for g in gnbs], marker="^", s=120, c="k", label="gNB")
for g in gnbs:
    ax.annotate(f"cell {g['CellId']}", (float(g["X_m"]), float(g["Y_m"])), textcoords="offset points", xytext=(0, -16), ha="center")
for u in static_ues:
    t, x = series(runs["with handover"], u, "UeX_m")
    _, y = series(runs["with handover"], u, "UeY_m")
    ax.scatter(x[:1], y[:1], marker="o", c="tab:orange", label="static UEs" if u == static_ues[0] else None)
t, x = series(runs["with handover"], 0, "UeX_m")
_, y = series(runs["with handover"], 0, "UeY_m")
ax.plot(x, y, "g.-", label="moving UE")
ax.set_xlabel("x [m]")
ax.set_ylabel("y [m]")
ax.set_title("gNB and UE locations")
ax.legend()
axs[0][1].axis("off")

metrics = [("RxMbps", "Received VR bitrate [Mbps]"), ("MeanDelay_ms", "Mean VR delay [ms]"), ("LossPercent", "VR packet loss [%]"), ("ServingCell", "Serving cell")]
for row, (col, ylabel) in enumerate(metrics, start=1):
    for c, (label, color) in enumerate([("with handover", "tab:blue"), ("without handover", "tab:red")]):
        ax = axs[row][c]
        for u in ues:
            t, v = series(runs[label], u, col)
            kw = {"lw": 2.2, "color": color, "label": "moving UE"} if u == 0 else {"lw": 1, "alpha": 0.7, "label": f"static UE {u}"}
            if col == "ServingCell":
                ax.step(t, v, where="post", **kw) if u == 0 else None
            else:
                ax.plot(t, v, **kw)
        if col == "MeanDelay_ms":
            ax.set_yscale("log")
        ax.set_title(f"{label}")
        ax.set_xlabel("time [s]")
        ax.set_ylabel(ylabel)
        ax.grid(True, alpha=0.3)
        if col != "ServingCell":
            ax.legend(fontsize=7)
    # same y range on both columns
    lo = min(axs[row][0].get_ylim()[0], axs[row][1].get_ylim()[0])
    hi = max(axs[row][0].get_ylim()[1], axs[row][1].get_ylim()[1])
    for c in range(2):
        axs[row][c].set_ylim(lo, hi)

fig.suptitle("oran-util-udp: moving UE plus static background UEs at each gNB")
fig.tight_layout()
fig.savefig(out, dpi=110)
print("wrote", out)
