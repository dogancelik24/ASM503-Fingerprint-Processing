import csv
from pathlib import Path
from statistics import NormalDist

import matplotlib.pyplot as plt

ROOT = Path("C:/ASM503")
RESULTS_DIR = ROOT / "bin" / "results"
IMAGE_DIR = ROOT / "images"

DATASETS = [
    "FVC2000DB1",
    "FVC2002DB1",
    "FVC2004DB1",
]

normal = NormalDist()

def probit(p):
    p = min(max(p, 0.00001), 0.99999)
    return normal.inv_cdf(p)


def calculate_eer(results):
    best = min(results, key=lambda x: abs(x["far"] - x["frr"]))

    eer = (best["far"] + best["frr"]) / 2
    eer_threshold = float(best["threshold"])

    for i in range(len(results) - 1):
        a = results[i]
        b = results[i + 1]

        diff_a = a["far"] - a["frr"]
        diff_b = b["far"] - b["frr"]

        if diff_a == 0:
            eer = (a["far"] + a["frr"]) / 2
            eer_threshold = float(a["threshold"])
            break

        if diff_a * diff_b < 0:
            ratio = abs(diff_a) / (abs(diff_a) + abs(diff_b))

            far_cross = a["far"] + ratio * (b["far"] - a["far"])
            frr_cross = a["frr"] + ratio * (b["frr"] - a["frr"])

            eer = (far_cross + frr_cross) / 2
            eer_threshold = a["threshold"] + ratio * (
                b["threshold"] - a["threshold"]
            )
            break

    return eer, eer_threshold


tick_percentages = [
    0.001, 0.01, 0.1, 0.5, 1, 2, 5, 10,
    20, 40, 60, 80, 90, 99
]

tick_positions = [
    probit(p / 100) for p in tick_percentages
]

tick_labels = [
    f"{p:g}" for p in tick_percentages
]

IMAGE_DIR.mkdir(parents=True, exist_ok=True)

for dataset in DATASETS:
    input_file = RESULTS_DIR / f"{dataset}_det_curve.csv"
    output_file = IMAGE_DIR / f"{dataset}_det.png"

    results = []

    with input_file.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            results.append({
                "threshold": int(row["threshold"]),
                "far": float(row["far"]),
                "frr": float(row["frr"]),
            })

    if not results:
        raise SystemExit(f"{dataset}: DET verisi bulunamadi!")

    eer, eer_threshold = calculate_eer(results)

    x = [probit(r["far"]) for r in results]
    y = [probit(r["frr"]) for r in results]

    eer_position = probit(eer)

    plt.figure(figsize=(8, 8))
    plt.plot(x, y, linewidth=1.8, label="DET Curve")
    plt.plot(
        [min(tick_positions), max(tick_positions)],
        [min(tick_positions), max(tick_positions)],
        "--",
        linewidth=1,
        label="FAR = FRR"
    )
    plt.scatter(
        [eer_position],
        [eer_position],
        s=65,
        zorder=5,
        label=f"EER = {eer * 100:.4f}%"
    )

    plt.xticks(tick_positions, tick_labels)
    plt.yticks(tick_positions, tick_labels)
    plt.xlim(min(tick_positions), max(tick_positions))
    plt.ylim(min(tick_positions), max(tick_positions))

    plt.xlabel("False Acceptance Rate (FAR) %")
    plt.ylabel("False Rejection Rate (FRR) %")
    plt.title(f"{dataset} - DET Curve")
    plt.grid(True, linestyle=":", alpha=0.6)
    plt.legend(loc="best")
    plt.tight_layout()

    plt.savefig(output_file, dpi=300)
    plt.close()

    print(f"{dataset}: EER = %{eer * 100:.4f}")
    print(f"EER esigi: {eer_threshold:.4f}")
    print(f"Grafik: {output_file}")
    print()

print("Uc veri setinin DET grafigi olusturuldu.")
