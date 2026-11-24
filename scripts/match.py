from pathlib import Path
from itertools import combinations
import subprocess
import csv
import re
import sys

ROOT = Path("C:/ASM503")
BOZORTH3 = ROOT / "NBIS" / "bin" / "bozorth3.exe"
OUTPUT_DIR = ROOT / "bin" / "results"

DATASETS = {
    "FVC2000DB1": Path("C:/ASM503_external/FVC2000_DB1_B_gabor"),
    "FVC2002DB1": Path("C:/ASM503_external/FVC2002DB1_gabor"),
    "FVC2004DB1": Path("C:/ASM503_external/FVC2004_DB1_B_gabor"),
}

if not BOZORTH3.exists():
    sys.exit(f"Bozorth3 bulunamadi: {BOZORTH3}")

OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

total_failed = 0

for dataset_name, xyt_dir in DATASETS.items():

    print(f"\n{'=' * 60}")
    print(f"VERI SETI: {dataset_name}")
    print(f"{'=' * 60}")

    files = sorted(xyt_dir.glob("*.xyt"))

    if len(files) != 80:
        sys.exit(
            f"{dataset_name}: 80 XYT bekleniyordu, "
            f"bulunan: {len(files)} ({xyt_dir})"
        )

    pairs = list(combinations(files, 2))

    csv_output = OUTPUT_DIR / f"{dataset_name}_match_scores.csv"
    genuine_output = OUTPUT_DIR / f"{dataset_name}_genuine_scores.txt"
    impostor_output = OUTPUT_DIR / f"{dataset_name}_impostor_scores.txt"

    genuine_scores = []
    impostor_scores = []
    failed_count = 0

    with csv_output.open("w", newline="", encoding="utf-8") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(["file1", "file2", "type", "score"])

        for index, (f1, f2) in enumerate(pairs, 1):

            id1 = f1.stem.split("_")[0]
            id2 = f2.stem.split("_")[0]

            match_type = "genuine" if id1 == id2 else "impostor"

            result = subprocess.run(
                [str(BOZORTH3), str(f1), str(f2)],
                capture_output=True,
                text=True
            )

            if result.returncode != 0:
                failed_count += 1
                print(f"HATA: {f1.name} - {f2.name}")
                print(result.stderr)
                continue

            numbers = re.findall(r"-?\d+", result.stdout)

            if not numbers:
                failed_count += 1
                print(f"SKOR OKUNAMADI: {f1.name} - {f2.name}")
                continue

            score = int(numbers[-1])

            writer.writerow([f1.name, f2.name, match_type, score])

            if match_type == "genuine":
                genuine_scores.append(score)
            else:
                impostor_scores.append(score)

            if index % 500 == 0:
                print(f"Ilerleme: {index}/{len(pairs)}")

    with genuine_output.open("w", encoding="utf-8") as f:
        for score in genuine_scores:
            f.write(f"{score}\n")

    with impostor_output.open("w", encoding="utf-8") as f:
        for score in impostor_scores:
            f.write(f"{score}\n")

    print("\nSONUC:")
    print(f"XYT dosyasi          : {len(files)}")
    print(f"Toplam karsilastirma: {len(pairs)}")
    print(f"Genuine              : {len(genuine_scores)}")
    print(f"Impostor             : {len(impostor_scores)}")
    print(f"Basarisiz            : {failed_count}")
    print(f"CSV                  : {csv_output}")
    print(f"Genuine TXT          : {genuine_output}")
    print(f"Impostor TXT         : {impostor_output}")

    total_failed += failed_count

print("\n" + "=" * 60)
print("TUM VERI SETLERI TAMAMLANDI")
print("Toplam basarisiz karsilastirma:", total_failed)
print("=" * 60)

if total_failed:
    sys.exit(1)
