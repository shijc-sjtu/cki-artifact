#!/bin/python

import matplotlib.pyplot as plt
import numpy as np
import os
import matplotlib as mpl
import matplotlib.gridspec as gridspec

from common import *


fontsize = 11
latex_col = 241.02039  ## pt

sbar_space = 0.05
sbar_wz = 0.3 - sbar_space
n_sbar = 3
whole_bar_width = (sbar_space + sbar_wz) * (n_sbar - 1)
width = whole_bar_width  # the width of the bars: can also be len(x) sequence
linewidth = 0.5

mpl.rcParams["hatch.linewidth"] = 0.5  # previous pdf hatch

marker_sz = 5
marker_edge_w = linewidth

PVM = "PVM"
HVM = "HVM"
RUNC = "RunC"
CKI = "CKI"

colors = {
    PVM: "#4292C6",
    CKI: "#6BAED6",
    HVM: "#9ECAE1",
    RUNC: "#C6DBEF",
}

names = [
    "fillseq",
    "fillseqbatch",
    "fillrandom",
    "fillrandbatch",
    "overwritebatch",
    "readseq",
    "readrandom",
]

def collect(log_file):
    data = []
    data_tmp = []
    cur = 0
    expected_names = ["fillseq"] + names
    with open(log_file) as file:
        for line in file.readlines():
            if not line.split():
                continue
            elif line.split()[0] == expected_names[cur]:
                cur += 1
                data_tmp.append(float(line.split()[2]))
            elif line.split()[0] == expected_names[0]:
                cur = 1
                data_tmp = [float(line.split()[2])]
            if len(data_tmp) == 8:
                data += data_tmp
                cur = 0
                data_tmp = []
    assert(len(data) == 40)
    avg_data = []
    for i in range(1, 8):
        avg_data.append(sum(data[i:40:8]) / 5)
    return avg_data
    
values = {
    PVM: collect("log/run_fig14_pvm.log"),
    CKI: collect("log/run_fig14_cki.log"),
    HVM: collect("log/run_fig14_hvm.log"),
}

def plot(fig, spec):
    ax =  fig.add_subplot(spec)

    labels = np.arange(start=-1, stop=-1+1.5*len(names), step=1.5)

    normalized_values = {
        PVM: [values[CKI][i] / values[PVM][i] for i in range(len(names))],
        CKI: [values[CKI][i] / values[CKI][i] for i in range(len(names))],
        HVM: [values[CKI][i] / values[HVM][i] for i in range(len(names))],
        # RUNC: [values[RUNC][i] / values[RUNC][i] for i in range(len(names))],
    }

    for i, k in enumerate(values):
        ax.bar(
            labels + (sbar_wz + sbar_space) * i,
            normalized_values[k],
            sbar_wz,
            label=k,
            # hatch=hatch,
            lw=0.05,
            edgecolor="black",
            color=colors[k],
            zorder=3,
            # yerr=([1 * i for i in app.err[k]], (app.err[k])),
            # yerr = yerr,
            error_kw={"linewidth": 0.7, "capsize": 1, "capthick": 0.5},
        )
    
    leg = ax.legend(
        fontsize=fontsize,
        frameon=False,
        loc="upper center",
        handlelength=1.2,
        labelspacing=0.1,
        ncol=5,
        bbox_to_anchor=(0.46, 1.35),
        handletextpad=0.3,
    )
    leg.set_zorder(1)

    adjust_ax_style(ax)

    ax.set_xticks(
        (labels + whole_bar_width / 2)[0 : len(names)],
        names,
        rotation=10,
        fontsize=fontsize,
        # fontproperties=font,
        #labelpad = -5,
    )     
    ax.tick_params(axis='x', labelsize=fontsize)
    ax.tick_params(axis="y", labelsize=fontsize)

    ax.set_ylabel("Throughput\n(Normalized)", fontsize=fontsize)


# with plt.style.context(["science", "high-vis", "no-latex"]):
if __name__ == "__main__":
    plt.rcParams["lines.markersize"] = 3
    plt.rcParams["font.family"] = 'sans-serif'

    fig = plt.figure(constrained_layout=False)
    spec = gridspec.GridSpec(ncols=1, nrows=1, figure=fig)

    plot(fig, spec[0, 0])

    fig.set_size_inches(get_figsize(latex_col, wf=1.0 * 2 + 0.05, hf=0.105 * 3))
    fig.subplots_adjust(wspace=0.12, hspace=0.18)

    save_figure(fig, f"plots/fig14")
