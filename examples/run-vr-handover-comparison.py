#!/usr/bin/env python3
"""Run vr-nr-oran-handover-bg for several adaptation algorithms with and without
handover, and plot the comparison.

Scenarios: {oran-util-udp, oran-util-udp-no-queue, fuzzy} x {handover on, off}.
Every run uses the same RNG seed and its own working directory, so the runs can be
executed in parallel.

Usage (from the ns-3 root directory):
    contrib/ns-3-adaptive-vr-app/examples/run-vr-handover-comparison.py --seed 1 --jobs 3
    ... --plot-only          # re-plot existing results without running the simulations
    ... -- --simTime=22      # arguments after "--" are passed to the simulation

Outputs in <out-dir> (default vr-comparison-seed<seed>):
    <algorithm>-ho-{on,off}/run-*.csv   raw per-run CSV files
    comparison-moving-ue.png            moving UE: bitrate, delay, loss, serving cell
    comparison-static-ues.png           static UEs (with handover on): bitrate, delay
    comparison-summary.png              bar chart of the summary statistics
    summary.csv                         summary statistics of every run
"""
import argparse
import concurrent.futures
import csv
import os
import statistics
import subprocess
import sys

ALGORITHMS = ["oran-util-udp", "oran-util-udp-no-queue", "fuzzy"]
HANDOVER = [("on", "true"), ("off", "false")]
COLORS = {"on": "tab:blue", "off": "tab:red"}


def run_dir(out_dir, alg, ho):
    return os.path.join(out_dir, f"{alg}-ho-{ho}")


def run_one(args, alg, ho, ho_flag, extra):
    """Run a single simulation in its own directory."""
    d = os.path.abspath(run_dir(args.out_dir, alg, ho))
    os.makedirs(d, exist_ok=True)
    sim = (
        f"{args.example} --burstGeneratorType={alg} --handover={ho_flag} "
        f"--appRate={args.app_rate} --outPrefix=run --dbFile=run.db --RngRun={args.seed} "
        + " ".join(extra)
    )
    cmd = [os.path.join(args.ns3_dir, "ns3"), "run", "--no-build", f"--cwd={d}", sim]
    with open(os.path.join(d, "stdout.log"), "w") as log:
        rc = subprocess.call(cmd, stdout=log, stderr=subprocess.STDOUT, cwd=args.ns3_dir)
    for f in os.listdir(d):
        if f.endswith(".db") or f.endswith(".db-wal") or f.endswith(".db-shm"):
            os.remove(os.path.join(d, f))
    print(f"[{'ok' if rc == 0 else 'FAILED'}] {alg} handover={ho_flag} (log: {d}/stdout.log)", flush=True)
    return rc


def read(path):
    with open(path) as f:
        return list(csv.DictReader(f))


def ue_series(rows, ue, col):
    r = [x for x in rows if int(x["UeId"]) == ue]
    return [float(x["Time_s"]) for x in r], [float(x[col]) for x in r]


def load(out_dir):
    """Load the time series of all runs that finished."""
    runs = {}
    for alg in ALGORITHMS:
        for ho, _ in HANDOVER:
            p = os.path.join(run_dir(out_dir, alg, ho), "run-timeseries.csv")
            if os.path.exists(p):
                runs[(alg, ho)] = read(p)
            else:
                print(f"warning: missing {p}", file=sys.stderr)
    return runs


def count_handovers(rows):
    cells = [int(r["ServingCell"]) for r in rows if int(r["UeId"]) == 0]
    return sum(1 for a, b in zip(cells, cells[1:]) if a != b)


def summarize(runs, out_dir):
    """Write summary.csv and return the summary as a dict."""
    summary = {}
    for (alg, ho), rows in runs.items():
        mv = [r for r in rows if int(r["UeId"]) == 0 and float(r["RxMbps"]) > 0]
        st = [r for r in rows if int(r["UeId"]) != 0 and float(r["RxMbps"]) > 0]
        mv_all = [r for r in rows if int(r["UeId"]) == 0]
        sent = sum(int(r["SentPackets"]) for r in mv_all)
        lost = sum(int(r["LostPackets"]) for r in mv_all)
        delays = [float(r["MeanDelay_ms"]) for r in mv]
        summary[(alg, ho)] = {
            "mov_mean_mbps": statistics.mean(float(r["RxMbps"]) for r in mv_all) if mv_all else 0,
            "mov_mean_delay_ms": statistics.mean(delays) if delays else 0,
            "mov_max_delay_ms": max(delays) if delays else 0,
            "mov_loss_pct": 100.0 * lost / sent if sent else 0,
            "mov_lost_pkts": lost,
            "static_mean_mbps": statistics.mean(float(r["RxMbps"]) for r in st) if st else 0,
            "static_mean_delay_ms": statistics.mean(float(r["MeanDelay_ms"]) for r in st) if st else 0,
            "handovers": count_handovers(rows),
        }
    cols = ["algorithm", "handover"] + list(next(iter(summary.values())).keys()) if summary else []
    with open(os.path.join(out_dir, "summary.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(cols)
        for (alg, ho), s in summary.items():
            w.writerow([alg, ho] + [f"{v:.3f}" if isinstance(v, float) else v for v in s.values()])
    for (alg, ho), s in summary.items():
        print(
            f"{alg:24s} ho={ho:3s} mov rate {s['mov_mean_mbps']:6.2f} Mbps, delay mean/max "
            f"{s['mov_mean_delay_ms']:7.1f}/{s['mov_max_delay_ms']:7.1f} ms, loss "
            f"{s['mov_loss_pct']:5.2f}% ({s['mov_lost_pkts']} pkts), handovers {s['handovers']}"
        )
    return summary


def plot(runs, summary, out_dir):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    algs = [a for a in ALGORITHMS if any((a, h) in runs for h, _ in HANDOVER)]
    metrics = [
        ("RxMbps", "Received VR bitrate [Mbps]"),
        ("MeanDelay_ms", "Mean VR delay [ms]"),
        ("LossPercent", "VR packet loss [%]"),
        ("ServingCell", "Serving cell"),
    ]

    # Moving UE
    fig, axs = plt.subplots(len(metrics), len(algs), figsize=(5.5 * len(algs), 3.2 * len(metrics)), squeeze=False)
    for c, alg in enumerate(algs):
        for r, (col, ylabel) in enumerate(metrics):
            ax = axs[r][c]
            for ho, _ in HANDOVER:
                if (alg, ho) not in runs:
                    continue
                t, v = ue_series(runs[(alg, ho)], 0, col)
                if col == "MeanDelay_ms":
                    # intervals without received packets have no delay
                    v = [x if x > 0 else float("nan") for x in v]
                kw = dict(color=COLORS[ho], label=f"handover {ho}")
                if col == "ServingCell":
                    ax.step(t, v, where="post", **kw)
                else:
                    ax.plot(t, v, lw=1.6, **kw)
            if col == "MeanDelay_ms":
                ax.set_yscale("log")
            if r == 0:
                ax.set_title(alg + (" (TCP, loss hidden)" if alg == "fuzzy" else ""))
            ax.set_ylabel(ylabel)
            ax.set_xlabel("time [s]")
            ax.grid(True, alpha=0.3)
            ax.legend(fontsize=7)
        for r, (col, _) in enumerate(metrics[:3]):
            lo = min(axs[r][k].get_ylim()[0] for k in range(len(algs)))
            if col == "MeanDelay_ms":
                lo = max(lo, 0.1)
            hi = max(axs[r][k].get_ylim()[1] for k in range(len(algs)))
            for k in range(len(algs)):
                axs[r][k].set_ylim(lo, hi)
    fig.suptitle("Moving UE")
    fig.tight_layout()
    fig.savefig(os.path.join(out_dir, "comparison-moving-ue.png"), dpi=110)
    plt.close(fig)

    # Static UEs, with handover on
    fig, axs = plt.subplots(2, len(algs), figsize=(5.5 * len(algs), 6.4), squeeze=False)
    for c, alg in enumerate(algs):
        rows = runs.get((alg, "on"))
        if rows is None:
            continue
        ues = sorted({int(r["UeId"]) for r in rows if int(r["UeId"]) != 0})
        for r, (col, ylabel) in enumerate(metrics[:2]):
            for u in ues:
                t, v = ue_series(rows, u, col)
                axs[r][c].plot(t, v, lw=1, label=f"static UE {u}")
            axs[r][c].set_ylabel(ylabel)
            axs[r][c].set_xlabel("time [s]")
            axs[r][c].grid(True, alpha=0.3)
            axs[r][c].legend(fontsize=7)
            if col == "MeanDelay_ms":
                axs[r][c].set_yscale("log")
        axs[0][c].set_title(alg)
    fig.suptitle("Static background UEs (handover enabled)")
    fig.tight_layout()
    fig.savefig(os.path.join(out_dir, "comparison-static-ues.png"), dpi=110)
    plt.close(fig)

    # Summary bars
    bars = [
        ("mov_mean_mbps", "Moving UE mean bitrate [Mbps]"),
        ("mov_mean_delay_ms", "Moving UE mean delay [ms]"),
        ("mov_max_delay_ms", "Moving UE max delay [ms]"),
        ("mov_loss_pct", "Moving UE packet loss [%]"),
    ]
    fig, axs = plt.subplots(1, len(bars), figsize=(4.5 * len(bars), 4))
    width = 0.38
    for ax, (key, title) in zip(axs, bars):
        for i, (ho, _) in enumerate(HANDOVER):
            vals = [summary.get((a, ho), {}).get(key, 0) for a in algs]
            ax.bar([x + (i - 0.5) * width for x in range(len(algs))], vals, width, color=COLORS[ho], label=f"handover {ho}")
        ax.set_xticks(range(len(algs)))
        ax.set_xticklabels([a.replace("oran-util-udp", "util-udp") for a in algs], rotation=15)
        ax.set_title(title, fontsize=10)
        if "delay" in key:
            ax.set_yscale("log")
        ax.grid(True, axis="y", alpha=0.3)
    axs[0].legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(os.path.join(out_dir, "comparison-summary.png"), dpi=110)
    plt.close(fig)
    print("wrote plots to", out_dir)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    default_ns3 = os.path.abspath(os.path.join(here, "..", "..", ".."))
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seed", type=int, default=1, help="RNG run number used by every simulation")
    ap.add_argument("--jobs", type=int, default=3, help="number of simulations to run in parallel")
    ap.add_argument("--out-dir", default=None, help="output directory (default vr-comparison-seed<seed>)")
    ap.add_argument("--example", default="vr-nr-oran-handover-bg", help="ns-3 example to run")
    ap.add_argument("--app-rate", default="50Mbps", help="VR application target data rate")
    ap.add_argument("--ns3-dir", default=default_ns3, help="ns-3 root directory")
    ap.add_argument("--plot-only", action="store_true", help="do not run, only plot existing results")
    ap.add_argument("--algorithms", nargs="+", default=ALGORITHMS, choices=ALGORITHMS)
    args, extra = ap.parse_known_args()
    extra = [e for e in extra if e != "--"]
    ALGORITHMS[:] = args.algorithms
    args.out_dir = os.path.abspath(args.out_dir or f"vr-comparison-seed{args.seed}")
    os.makedirs(args.out_dir, exist_ok=True)

    if not args.plot_only:
        subprocess.check_call([os.path.join(args.ns3_dir, "ns3"), "build", args.example], cwd=args.ns3_dir)
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            futs = [
                pool.submit(run_one, args, alg, ho, flag, extra)
                for alg in ALGORITHMS
                for ho, flag in HANDOVER
            ]
            if any(f.result() != 0 for f in futs):
                print("some simulations failed", file=sys.stderr)

    runs = load(args.out_dir)
    if not runs:
        sys.exit("no results found")
    summary = summarize(runs, args.out_dir)
    plot(runs, summary, args.out_dir)


if __name__ == "__main__":
    main()
