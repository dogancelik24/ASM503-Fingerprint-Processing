import csv
from pathlib import Path

ROOT = Path("C:/ASM503")
RESULTS_DIR = ROOT / "bin" / "results"

DATASETS = [
    "FVC2000DB1",
    "FVC2002DB1",
    "FVC2004DB1",
]

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

    return best, eer, eer_threshold


for dataset in DATASETS:
    input_file = RESULTS_DIR / f"{dataset}_match_scores.csv"
    output_file = RESULTS_DIR / f"{dataset}_det_curve.csv"

    genuine = []
    impostor = []

    with input_file.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            score = int(row["score"])

            if row["type"] == "genuine":
                genuine.append(score)
            elif row["type"] == "impostor":
                impostor.append(score)

    if not genuine or not impostor:
        raise SystemExit(f"{dataset}: Genuine veya impostor verisi yok!")

    max_score = max(genuine + impostor)
    results = []

    for threshold in range(max_score + 2):
        false_accepts = sum(s >= threshold for s in impostor)
        false_rejects = sum(s < threshold for s in genuine)

        results.append({
            "threshold": threshold,
            "far": false_accepts / len(impostor),
            "frr": false_rejects / len(genuine)
        })

    with output_file.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=["threshold", "far", "frr"]
        )
        writer.writeheader()
        writer.writerows(results)

    best, eer, eer_threshold = calculate_eer(results)

    print(f"\n===== {dataset} DET / EER =====")
    print(f"Genuine sayisi : {len(genuine)}")
    print(f"Impostor sayisi: {len(impostor)}")
    print(f"Toplam esik    : {len(results)}")
    print(f"En yakin esik  : {best['threshold']}")
    print(f"Bu esikte FAR  : %{best['far'] * 100:.4f}")
    print(f"Bu esikte FRR  : %{best['frr'] * 100:.4f}")
    print(f"EER            : %{eer * 100:.4f}")
    print(f"EER esigi      : {eer_threshold:.4f}")
    print(f"DET CSV        : {output_file}")

print("\nTum veri setlerinin DET hesaplamasi tamamlandi.")
