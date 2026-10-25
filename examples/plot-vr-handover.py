#!/usr/bin/env python3
"""Plot the output of vr-nr-oran-handover for runs with and without handover.

Usage: plot-vr-handover.py <prefix-with-handover> <prefix-without-handover> [output.png]
"""
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import csv

with_ho, without_ho = sys.argv[1], sys.argv[2]
out = sys.argv[3] if len(sys.argv) > 3 else "vr-handover-comparison.png"

runs = [("with handover", with_ho, "tab:blue"), ("without handover", without_ho, "tab:red")]


def read(name):
    """Read a CSV file into a dict of column name -> list of floats."""
    with open(name) as f:
        rows = list(csv.DictReader(f))
    return {k: [float(r[k]) for r in rows] for k in rows[0]}


data = {label: read(f"{p}-timeseries.csv") for label, p, _ in runs}
gnbs = read(f"{with_ho}-gnb-positions.csv")

fig, axs = plt.subplots(5, 1, figsize=(10, 15), sharex=False)

# 1. Positions: gNBs and the UE trajectory
ax = axs[0]
ax.scatter(gnbs["X_m"], gnbs["Y_m"], marker="^", s=120, c="k", label="gNB")
for cid, x, y in zip(gnbs["CellId"], gnbs["X_m"], gnbs["Y_m"]):
    ax.annotate(f"cell {int(cid)}", (x, y), textcoords="offset points", xytext=(0, 8), ha="center")
ue = data["with handover"]
ax.plot(ue["UeX_m"], ue["UeY_m"], "g.-", label="UE trajectory")
ax.set_xlabel("x [m]")
ax.set_ylabel("y [m]")
ax.set_title("gNB and UE locations")
ax.legend()

# 2-5. Time series
series = [
    ("RxMbps", "Received VR bitrate [Mbps]"),
    ("MeanDelay_ms", "Mean VR delay [ms]"),
    ("LossPercent", "VR packet loss [%]"),
    ("ServingCell", "Serving cell"),
]
for ax, (col, ylabel) in zip(axs[1:], series):
    for label, _, color in runs:
        df = data[label]
        if col == "ServingCell":
            ax.step(df["Time_s"], df[col], where="post", label=label, color=color)
        else:
            ax.plot(df["Time_s"], df[col], label=label, color=color)
    ax.set_xlabel("time [s]")
    ax.set_ylabel(ylabel)
    ax.grid(True, alpha=0.3)
    ax.legend()
axs[2].set_yscale("log")
ho = data["with handover"]
cells = ho["ServingCell"]
changes = [ho["Time_s"][i] for i in range(1, len(cells)) if cells[i] != cells[i - 1]]
for ax in axs[1:]:
    for t in changes:
        ax.axvline(t, color="tab:blue", ls=":", alpha=0.6)
axs[1].set_title("VR downlink at the UE (dotted lines: handovers)")

fig.tight_layout()
fig.savefig(out, dpi=120)
print("wrote", out)
