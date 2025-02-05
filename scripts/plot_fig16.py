#!/bin/python

import matplotlib.pyplot as plt
from common import *
import json

fontsize = 11
latex_col = 241.02039  ## pt
linewidth = 0.7
plt.rcParams['lines.markersize'] = 3

marker_sz = 4
marker_edge_w = linewidth

def collect(log_file):
    data = []
    with open(log_file) as file:
        for line in file.readlines():
            if "Totals" in line:
                data.append(float(line.split()[1]) / 1000)
    return data

# Memcached
n = [1,2,3,4,5,6,7,8,9,10,11,12]
cki = collect("log/run_fig16_cki_memcached.log")
pvm = collect("log/run_fig16_pvm_memcached.log")
hvm = collect("log/run_fig16_hvm_memcached.log")

fig, (ax, ax2) = plt.subplots(1, 2)
fig.set_size_inches(get_figsize(latex_col, wf=1.0 * 2, hf=0.105 * 5))

ax.plot(
    n,
    pvm,
    markerfacecolor="none",
    linestyle="solid",
    label="PVM-NST",
    linewidth=linewidth,
    color=color_list[5],
    marker="^",
    markersize=marker_sz,
    markeredgewidth=marker_edge_w,
    zorder=3,
)

ax.plot(
    n,
    cki,
    markerfacecolor="none",
    linestyle="solid",
    label="CKI-NST",
    linewidth=linewidth,
    color=color_list[0],
    marker="+",
    markersize=marker_sz,
    markeredgewidth=marker_edge_w,
    zorder=3,
)

ax.plot(
    n,
    hvm,
    markerfacecolor="none",
    linestyle="solid",
    label="HVM-NST",
    linewidth=linewidth,
    color=color_list[3],
    marker="o",
    markersize=marker_sz,
    markeredgewidth=marker_edge_w,
    zorder=3,
)

# ax.plot(
#     n,
#     runc,
#     markerfacecolor="none",
#     linestyle="solid",
#     label="RunC-NST",
#     linewidth=linewidth,
#     color=color_list[4],
#     marker="v",
#     markersize=marker_sz,
#     markeredgewidth=marker_edge_w,
#     zorder=3,
# )

ax.legend(bbox_to_anchor=(1.95, 1.25), ncol=4, fontsize=fontsize, frameon=False)

ax.set_xticks([2, 4, 6, 8, 10, 12], [2, 4, 6, 8, 10, 12], fontsize=fontsize)
ax.set_xlabel('#Clients', fontsize=fontsize)
ax.set_ylabel('Throughput (Kops)', fontsize=fontsize)

ax.yaxis.grid(color="black", linestyle=(0, (5, 10)), linewidth=0.1, zorder=0)
ax.xaxis.grid(color="black", linestyle=(0, (5, 10)), linewidth=0.1, zorder=0)

ax.tick_params(which='major', direction='in', length=1, axis="x")
ax.tick_params(which='minor', direction='in', length=0, axis="x")
ax.tick_params(which='minor', direction='in', length=0, axis="y")
ax.tick_params(which='major', direction='in', length=2, axis="y")

ax = ax2


# Redis
n = [1,2,3,4,5,6,7,8,9,10,11,12]
cki = collect("log/run_fig16_cki_redis.log")
pvm = collect("log/run_fig16_pvm_redis.log")
hvm = collect("log/run_fig16_hvm_redis.log")

ax.plot(
    n,
    pvm,
    markerfacecolor="none",
    linestyle="solid",
    label="PVM-NST",
    linewidth=linewidth,
    color=color_list[5],
    marker="^",
    markersize=marker_sz,
    markeredgewidth=marker_edge_w,
    zorder=3,
)

ax.plot(
    n,
    cki,
    markerfacecolor="none",
    linestyle="solid",
    label="CKI-NST",
    linewidth=linewidth,
    color=color_list[0],
    marker="+",
    markersize=marker_sz,
    markeredgewidth=marker_edge_w,
    zorder=3,
)

ax.plot(
    n,
    hvm,
    markerfacecolor="none",
    linestyle="solid",
    label="HVM-NST",
    linewidth=linewidth,
    color=color_list[3],
    marker="o",
    markersize=marker_sz,
    markeredgewidth=marker_edge_w,
    zorder=3,
)

# ax.plot(
#     n,
#     runc,
#     markerfacecolor="none",
#     linestyle="solid",
#     label="RunC-NST",
#     linewidth=linewidth,
#     color=color_list[4],
#     marker="v",
#     markersize=marker_sz,
#     markeredgewidth=marker_edge_w,
#     zorder=3,
# )

ax.set_xticks([2, 4, 6, 8, 10, 12], [2, 4, 6, 8, 10, 12], fontsize=fontsize)
ax.set_xlabel('#Clients', fontsize=fontsize)

ax.yaxis.grid(color="black", linestyle=(0, (5, 10)), linewidth=0.1, zorder=0)
ax.xaxis.grid(color="black", linestyle=(0, (5, 10)), linewidth=0.1, zorder=0)

ax.tick_params(which='major', direction='in', length=1, axis="x")
ax.tick_params(which='minor', direction='in', length=0, axis="x")
ax.tick_params(which='minor', direction='in', length=0, axis="y")
ax.tick_params(which='major', direction='in', length=2, axis="y")


save_figure(fig, "plots/fig16")
