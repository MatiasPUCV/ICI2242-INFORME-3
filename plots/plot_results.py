"""Generate the figures of the pallet packing experiments from the CSV files."""

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from matplotlib.patches import Patch, Rectangle
from matplotlib.ticker import FuncFormatter, ScalarFormatter

ALGORITHM_ORDER = ["FF", "BFD", "VNS", "TABU"]
SEARCH_ORDER = ["VNS", "TABU"]
SCENARIOS = ["A", "B", "C"]

ALGORITHM_LABELS = {
    "FF": "First Fit (referencia)",
    "BFD": "Best Fit Decreasing",
    "VNS": "VNS",
    "TABU": "Búsqueda Tabú",
}
COLORS = {
    "FF": "#999999",
    "BFD": "#0072B2",
    "VNS": "#E69F00",
    "TABU": "#009E73",
}
HATCHES = {"FF": "..", "BFD": "//", "VNS": "xx", "TABU": "\\\\"}
MARKERS = {"FF": "s", "BFD": "o", "VNS": "^", "TABU": "D"}
LINE_STYLES = {"FF": ":", "BFD": "-", "VNS": "--", "TABU": "-."}

SCENARIO_LABELS = {
    "A": "Escenario A: cajas homogéneas",
    "B": "Escenario B: cajas heterogéneas",
    "C": "Escenario C: alta fragmentación",
}
CASE_ORDER = [
    "SIMPLE",
    "ROTATION",
    "MULTILAYER",
    "FRAGMENTATION",
    "INFEASIBLE",
    "REPORT_EXAMPLE",
]
CASE_LABELS = {
    "SIMPLE": "Empaquetamiento\nsencillo",
    "ROTATION": "Rotaciones",
    "MULTILAYER": "Múltiples\ncapas",
    "FRAGMENTATION": "Fragmentación",
    "INFEASIBLE": "Instancias\ninviables",
    "REPORT_EXAMPLE": "Ejemplo\ndel informe",
}
BOUND_LABEL = "Cota inferior por volumen"


def setup_style():
    """Configure matplotlib so that every figure is readable."""
    plt.rcParams.update(
        {
            "font.size": 13,
            "axes.titlesize": 14,
            "axes.labelsize": 13,
            "xtick.labelsize": 12,
            "ytick.labelsize": 12,
            "legend.fontsize": 12,
            "figure.titlesize": 17,
            "figure.dpi": 100,
            "savefig.dpi": 200,
            "axes.grid": True,
            "grid.alpha": 0.3,
            "axes.axisbelow": True,
            "axes.spines.top": False,
            "axes.spines.right": False,
            "lines.linewidth": 2.4,
            "lines.markersize": 9,
        }
    )


def format_number(value, decimals=None):
    """Format a number using the Spanish decimal comma."""
    if decimals is None:
        text = f"{value:g}"
    else:
        text = f"{value:.{decimals}f}"
    return text.replace(".", ",")


def comma_formatter():
    return FuncFormatter(lambda value, _: format_number(value))


def grouped_bars(ax, labels, series, errors=None):
    """Draw one group of bars per label and one bar per algorithm."""
    count = len(series)
    width = 0.8 / count
    positions = np.arange(len(labels))
    for index, key in enumerate(series):
        offset = (index - (count - 1) / 2) * width
        ax.bar(
            positions + offset,
            series[key],
            width,
            yerr=None if errors is None else errors[key],
            color=COLORS[key],
            hatch=HATCHES[key],
            edgecolor="black",
            linewidth=0.8,
            capsize=4,
            label=ALGORITHM_LABELS[key],
        )
    ax.set_xticks(positions)
    ax.set_xticklabels(labels)
    ax.yaxis.set_major_formatter(comma_formatter())
    return positions


def add_bottom_legend(fig, ax, extra_columns=0):
    handles, labels = ax.get_legend_handles_labels()
    fig.legend(
        handles,
        labels,
        loc="outside lower center",
        ncol=len(labels) + extra_columns,
        frameon=False,
    )


def save(fig, path):
    fig.savefig(path, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    print(f"Figura guardada: {path}")


def plot_validation(results, figures):
    data = pd.read_csv(results / "validation.csv")
    table = (
        data.groupby(["case", "algorithm"]).passed.mean().mul(100).unstack("algorithm")
    )
    table = table.loc[CASE_ORDER, ALGORITHM_ORDER]

    fig, ax = plt.subplots(figsize=(14, 6.5), layout="constrained")
    labels = [CASE_LABELS[case] for case in table.index]
    series = {key: table[key].to_numpy() for key in ALGORITHM_ORDER}
    positions = grouped_bars(ax, labels, series)

    width = 0.8 / len(series)
    for index, key in enumerate(series):
        offset = (index - (len(series) - 1) / 2) * width
        for position, value in zip(positions, series[key]):
            ax.text(
                position + offset,
                value + 1.5,
                f"{format_number(value, 0)} %",
                ha="center",
                va="bottom",
                fontsize=10,
                rotation=90,
            )

    ax.set_ylim(0, 125)
    ax.set_yticks(range(0, 101, 20))
    ax.set_ylabel("Soluciones correctas (%)")
    ax.set_title(
        "Experimento 1: validación de los algoritmos"
    )
    add_bottom_legend(fig, ax)
    save(fig, figures / "exp1_validacion.png")

    summary = data.groupby(["case", "algorithm"]).agg(
        instancias=("passed", "size"), correctas=("passed", "sum")
    )
    summary.to_csv(results / "resumen_validacion.csv")


def plot_quality_pallets(results, figures):
    data = pd.read_csv(results / "quality.csv")
    sizes = sorted(data["size"].unique())
    stats = data.groupby(["scenario", "size", "algorithm"]).pallets.agg(["mean", "std"])
    bounds = data.groupby(["scenario", "size", "instance"]).lowerBound.first()
    bounds = bounds.groupby(["scenario", "size"]).mean()

    fig, axes = plt.subplots(1, 3, figsize=(18, 6.5), layout="constrained")
    for ax, scenario in zip(axes, SCENARIOS):
        series = {}
        errors = {}
        for key in ALGORITHM_ORDER:
            series[key] = [stats.loc[(scenario, size, key), "mean"] for size in sizes]
            errors[key] = [stats.loc[(scenario, size, key), "std"] for size in sizes]
        positions = grouped_bars(ax, [f"{size} cajas" for size in sizes], series, errors)
        for position, size in zip(positions, sizes):
            ax.hlines(
                bounds.loc[(scenario, size)],
                position - 0.42,
                position + 0.42,
                colors="black",
                linestyles="--",
                linewidth=2.2,
                label=BOUND_LABEL if position == positions[0] else None,
                zorder=5,
            )
        ax.set_title(SCENARIO_LABELS[scenario])
        ax.set_ylabel("Pallets utilizados (promedio)")
        ax.set_ylim(bottom=0)

    fig.suptitle("Experimento 2: cantidad de pallets utilizados por algoritmo")
    add_bottom_legend(fig, axes[0], extra_columns=0)
    save(fig, figures / "exp2_pallets.png")

    stats.round(3).to_csv(results / "resumen_calidad_pallets.csv")


def plot_quality_improvement(results, figures):
    data = pd.read_csv(results / "quality.csv")
    sizes = sorted(data["size"].unique())
    wide = data.pivot_table(
        index=["scenario", "size", "instance"], columns="algorithm", values="pallets"
    ).reset_index()
    for key in SEARCH_ORDER:
        wide[key] = (wide["BFD"] - wide[key]) / wide["BFD"] * 100

    stats = wide.groupby(["scenario", "size"])[SEARCH_ORDER].agg(["mean", "sem"])
    instances = wide["instance"].nunique()
    top = float((stats.xs("mean", axis=1, level=1) + stats.xs("sem", axis=1, level=1)).max().max())
    top = max(top * 1.25, 5.0)

    fig, axes = plt.subplots(1, 3, figsize=(18, 6.5), layout="constrained", sharey=True)
    for ax, scenario in zip(axes, SCENARIOS):
        series = {
            key: [stats.loc[(scenario, size), (key, "mean")] for size in sizes]
            for key in SEARCH_ORDER
        }
        errors = {
            key: [stats.loc[(scenario, size), (key, "sem")] for size in sizes]
            for key in SEARCH_ORDER
        }
        positions = grouped_bars(ax, [f"{size} cajas" for size in sizes], series, errors)
        width = 0.8 / len(series)
        for index, key in enumerate(series):
            offset = (index - (len(series) - 1) / 2) * width
            for position, value, error in zip(positions, series[key], errors[key]):
                ax.text(
                    position + offset,
                    value + error + top * 0.02,
                    f"{format_number(value, 1)} %",
                    ha="center",
                    va="bottom",
                    fontsize=11,
                )
        ax.set_title(SCENARIO_LABELS[scenario])
        ax.set_ylabel("Mejora respecto a BFD (%)")
        ax.set_ylim(0, top)

    fig.suptitle(
        "Experimento 2: mejora de las búsquedas locales respecto a Best Fit Decreasing\n"
        f"Promedio de {instances} instancias por barra; barras de error: error estándar de la media"
    )
    add_bottom_legend(fig, axes[0])
    save(fig, figures / "exp2_mejora.png")

    stats.round(3).to_csv(results / "resumen_calidad_mejora.csv")


def plot_scalability_pallets(results, figures):
    data = pd.read_csv(results / "scalability.csv")
    sizes = sorted(data["size"].unique())
    per_instance = (
        data.groupby(["scenario", "size", "instance", "algorithm"])
        .pallets.mean()
        .unstack("algorithm")
        .reset_index()
    )
    mean_pallets = per_instance.groupby(["scenario", "size"])[ALGORITHM_ORDER].mean()
    bounds = (
        data.groupby(["scenario", "size", "instance"]).lowerBound.first().groupby(
            ["scenario", "size"]
        ).mean()
    )

    gains = per_instance.copy()
    for key in ["FF"] + SEARCH_ORDER:
        gains[key] = (gains["BFD"] - gains[key]) / gains["BFD"] * 100
    mean_gains = gains.groupby(["scenario", "size"])[["FF"] + SEARCH_ORDER].mean()

    fig, axes = plt.subplots(2, 3, figsize=(18, 11), layout="constrained")
    for ax, scenario in zip(axes[0], SCENARIOS):
        for key in ALGORITHM_ORDER:
            ax.plot(
                sizes,
                mean_pallets.loc[scenario][key].loc[sizes].to_numpy(),
                color=COLORS[key],
                marker=MARKERS[key],
                linestyle=LINE_STYLES[key],
                label=ALGORITHM_LABELS[key],
            )
        ax.plot(
            sizes,
            bounds.loc[scenario].loc[sizes].to_numpy(),
            color="black",
            linestyle="--",
            linewidth=1.8,
            label=BOUND_LABEL,
        )
        ax.set_title(SCENARIO_LABELS[scenario])
        ax.set_ylabel("Pallets utilizados (promedio)")

    for ax, scenario in zip(axes[1], SCENARIOS):
        for key in ["FF"] + SEARCH_ORDER:
            ax.plot(
                sizes,
                mean_gains.loc[scenario][key].loc[sizes].to_numpy(),
                color=COLORS[key],
                marker=MARKERS[key],
                linestyle=LINE_STYLES[key],
            )
        ax.axhline(0, color=COLORS["BFD"], linewidth=2.2, label=None)
        ax.set_ylabel("Mejora respecto a BFD (%)")

    for ax in axes.flat:
        ax.set_xscale("log")
        ax.set_xticks(sizes)
        ax.xaxis.set_major_formatter(ScalarFormatter())
        ax.xaxis.set_minor_formatter(plt.NullFormatter())
        ax.set_xlabel("Cantidad de cajas")
        ax.yaxis.set_major_formatter(comma_formatter())

    fig.suptitle(
        "Experimento 3: escalabilidad de la calidad "
        "(búsquedas locales: promedio de 10 restarts)"
    )
    add_bottom_legend(fig, axes[0][0])
    save(fig, figures / "exp3_escalabilidad_pallets.png")

    mean_pallets.round(3).to_csv(results / "resumen_escalabilidad_pallets.csv")


def plot_stability(results, figures):
    data = pd.read_csv(results / "scalability.csv")
    sizes = sorted(data["size"].unique())
    search = data[data["algorithm"].isin(SEARCH_ORDER)]
    deviation = (
        search.groupby(["scenario", "size", "instance", "algorithm"])
        .pallets.std(ddof=1)
        .groupby(["scenario", "size", "algorithm"])
        .mean()
    )

    top = max(0.3, float(deviation.max()) * 1.3)
    fig, axes = plt.subplots(1, 3, figsize=(18, 6.5), layout="constrained", sharey=True)
    for ax, scenario in zip(axes, SCENARIOS):
        series = {
            key: [deviation.loc[(scenario, size, key)] for size in sizes]
            for key in SEARCH_ORDER
        }
        positions = grouped_bars(ax, [f"{size} cajas" for size in sizes], series)
        width = 0.8 / len(series)
        for index, key in enumerate(series):
            offset = (index - (len(series) - 1) / 2) * width
            for position, value in zip(positions, series[key]):
                ax.text(
                    position + offset,
                    value + top * 0.02,
                    format_number(value, 2),
                    ha="center",
                    va="bottom",
                    fontsize=10,
                    rotation=90,
                )
        ax.set_title(SCENARIO_LABELS[scenario])
        ax.set_ylabel("Desviación estándar de pallets")
        ax.set_ylim(0, top)

    fig.suptitle(
        "Experimento 3: estabilidad entre los 10 restarts "
        "(promedio de la desviación estándar por instancia)"
    )
    add_bottom_legend(fig, axes[0])
    save(fig, figures / "exp3_estabilidad.png")

    deviation.round(4).to_csv(results / "resumen_estabilidad.csv")


def plot_runtime(results, figures):
    data = pd.read_csv(results / "scalability.csv")
    sizes = sorted(data["size"].unique())
    times = data.groupby(["scenario", "size", "algorithm"]).millis.mean()

    fig, axes = plt.subplots(1, 3, figsize=(18, 6.5), layout="constrained", sharey=True)
    for ax, scenario in zip(axes, SCENARIOS):
        for key in ALGORITHM_ORDER:
            ax.plot(
                sizes,
                [times.loc[(scenario, size, key)] for size in sizes],
                color=COLORS[key],
                marker=MARKERS[key],
                linestyle=LINE_STYLES[key],
                label=ALGORITHM_LABELS[key],
            )
        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_xticks(sizes)
        ax.xaxis.set_major_formatter(ScalarFormatter())
        ax.xaxis.set_minor_formatter(plt.NullFormatter())
        ax.yaxis.set_major_formatter(comma_formatter())
        ax.grid(True, which="both", alpha=0.25)
        ax.set_title(SCENARIO_LABELS[scenario])
        ax.set_xlabel("Cantidad de cajas")
        ax.set_ylabel("Tiempo por ejecución (ms)")

    fig.suptitle("Experimento 3: tiempo de ejecución (escala logarítmica en ambos ejes)")
    add_bottom_legend(fig, axes[0])
    save(fig, figures / "exp3_tiempo.png")

    times.round(4).to_csv(results / "resumen_tiempos.csv")


def plot_layouts(results, figures):
    path = results / "layout.csv"
    if not path.exists():
        return
    data = pd.read_csv(path)
    heights = sorted(data["height"].unique())
    palette = plt.get_cmap("Set2")
    height_colors = {height: palette(index) for index, height in enumerate(heights)}
    pallet_width, pallet_depth = 10, 8

    for algorithm in ALGORITHM_ORDER:
        subset = data[data["algorithm"] == algorithm]
        if subset.empty:
            continue
        pallet_count = subset["pallet"].max()
        layer_count = subset["layer"].max()
        fig, axes = plt.subplots(
            pallet_count,
            layer_count,
            figsize=(4.2 * layer_count, 3.9 * pallet_count + 1.2),
            squeeze=False,
            layout="constrained",
        )
        for ax in axes.flat:
            ax.set_visible(False)

        for (pallet, layer), group in subset.groupby(["pallet", "layer"]):
            ax = axes[pallet - 1][layer - 1]
            ax.set_visible(True)
            ax.set_xlim(0, pallet_width)
            ax.set_ylim(0, pallet_depth)
            ax.set_aspect("equal")
            ax.set_xticks(range(0, pallet_width + 1, 2))
            ax.set_yticks(range(0, pallet_depth + 1, 2))
            for row in group.itertuples():
                ax.add_patch(
                    Rectangle(
                        (row.x, row.y),
                        row.width,
                        row.depth,
                        facecolor=height_colors[row.height],
                        edgecolor="black",
                        linewidth=1.8,
                    )
                )
                ax.text(
                    row.x + row.width / 2,
                    row.y + row.depth / 2,
                    f"b{row.boxId + 1}",
                    ha="center",
                    va="center",
                    fontsize=13,
                    fontweight="bold",
                )
            z = group["z"].iloc[0]
            ax.set_title(f"Pallet {pallet} · capa {layer} (z = {z})", fontsize=13)
            ax.set_xlabel("Ancho")
            ax.set_ylabel("Profundidad")

        handles = [
            Patch(facecolor=height_colors[height], edgecolor="black", label=f"Altura {height}")
            for height in heights
        ]
        fig.legend(
            handles=handles,
            loc="outside lower center",
            ncol=len(handles),
            frameon=False,
        )
        fig.suptitle(
            f"Distribución - {ALGORITHM_LABELS[algorithm]} "
            f"({pallet_count} pallets)"
        )
        save(fig, figures / f"exp1_distribucion_{algorithm.lower()}.png")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--results", default="results", type=Path)
    parser.add_argument("--figures", default="figures", type=Path)
    arguments = parser.parse_args()

    arguments.figures.mkdir(parents=True, exist_ok=True)
    setup_style()

    if (arguments.results / "validation.csv").exists():
        plot_validation(arguments.results, arguments.figures)
        plot_layouts(arguments.results, arguments.figures)
    if (arguments.results / "quality.csv").exists():
        plot_quality_pallets(arguments.results, arguments.figures)
        plot_quality_improvement(arguments.results, arguments.figures)
    if (arguments.results / "scalability.csv").exists():
        plot_scalability_pallets(arguments.results, arguments.figures)
        plot_stability(arguments.results, arguments.figures)
        plot_runtime(arguments.results, arguments.figures)


if __name__ == "__main__":
    main()
