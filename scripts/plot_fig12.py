#!/bin/python

import matplotlib.pyplot as plt
import numpy as np
import os
import matplotlib as mpl
import matplotlib.gridspec as gridspec
from matplotlib import rc

from common import *


fontsize = 11
latex_col = 241.02039  ## pt

sbar_space = 0.05
sbar_wz = 0.25 - sbar_space
n_sbar = 3
whole_bar_width = (sbar_space + sbar_wz) * (n_sbar - 1)
width = whole_bar_width  # the width of the bars: can also be len(x) sequence
linewidth = 0.5

mpl.rcParams["hatch.linewidth"] = 0.5  # previous pdf hatch

PVM = "PVM"
HVM_NST = "HVM-NST"
HVM_BM = "HVM-BM"
RUNC = "RunC"
CKI = "CKI"

colors = {
    PVM: "#4292C6",
    CKI: "#9ECAE1",
    HVM_NST: "#2171B5",
    HVM_BM: "#6BAED6",
    RUNC: "#C6DBEF",
}

names = [
    "btree",
    "xsbench",
    "canneal",
    "dedup",
    "fluidanimate",
    "freqmine",
]

def collect(log_file):
    data = []
    with open(log_file) as file:
        for line in file.readlines():
            if "Took:" in line:
                data.append(float(line.split()[1]))
            elif "real" in line and "0m" in line:
                data.append(float(line.split()[2][:-1]))
    assert len(data) == 30
    avg_data = []
    for i in range(0, 30, 5):
        avg_data.append(sum(data[i:i+5]) / 5)
    return avg_data

values = {
    CKI: collect("log/run_fig12_cki.log"),
    PVM: collect("log/run_fig12_pvm.log"),
    HVM_NST: collect("log/run_fig12_hvm.log"),
}

def plot(fig, spec):
    ax =  fig.add_subplot(spec)

    labels = np.arange(start=-1, stop=-1+1.5*len(names), step=1.5)

    normalized_values = {
        HVM_NST: [values[HVM_NST][i] / values[HVM_NST][i] for i in range(len(names))],
        PVM: [values[PVM][i] / values[HVM_NST][i] for i in range(len(names))],
        CKI: [values[CKI][i] / values[HVM_NST][i] for i in range(len(names))],
    }

    for i, k in enumerate(values):
        ax.bar(
            labels + (sbar_wz + sbar_space) * i,
            normalized_values[k],
            sbar_wz,
            label=k,
            lw=0.05,
            edgecolor="black",
            color=colors[k],
            zorder=3,
            error_kw={"linewidth": 0.7, "capsize": 1, "capthick": 0.5},
        )
    
    leg = ax.legend(
        fontsize=fontsize,
        frameon=False,
        loc="upper center",
        handlelength=1.2,
        labelspacing=0.1,
        ncol=5,
        bbox_to_anchor=(0.48, 1.35),
        handletextpad=0.3,
    )
    leg.set_zorder(1)

    adjust_ax_style(ax)

    ax.set_xticks(
        (labels + whole_bar_width / 2)[0 : len(names)],
        names,
        fontsize=fontsize,
    )     
    ax.tick_params(axis='x', labelsize=fontsize)
    ax.tick_params(axis="y", labelsize=fontsize)

    ax.set_ylabel("Latency\n(Normalized)", fontsize=fontsize)


if __name__ == "__main__":
    plt.rcParams["lines.markersize"] = 3
    plt.rcParams["font.family"] = 'sans-serif'

    fig = plt.figure(constrained_layout=False)
    spec = gridspec.GridSpec(ncols=1, nrows=1, figure=fig)

    plot(fig, spec[0, 0])

    fig.set_size_inches(get_figsize(latex_col, wf=1.0 * 2 + 0.05, hf=0.105 * 3))
    fig.subplots_adjust(wspace=0.12, hspace=0.18)

    save_figure(fig, "plots/fig12")
