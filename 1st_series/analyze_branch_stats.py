import os
import re
import csv

# === Folder setup ===
# input_dir = "/home/john/Adv_Comp_Arch/advcomparch-ex1-helpcode/outputs_stats/train" 
input_dir = "/home/john/Adv_Comp_Arch/advcomparch-ex1-helpcode/outputs_stats/ref"
output_csv = "branch_stats_train.csv"

# === List with all the .out files ===
files = sorted([f for f in os.listdir(input_dir) if f.endswith(".out")])

# === Headers for CSV ===
header = [
    "Benchmark",
    "Total Instructions",
    "Total Branches",
    "Cond Taken (%)",
    "Cond Not Taken (%)",
    "Unconditional (%)",
    "Calls (%)",
    "Returns (%)"
]

rows = []

# === Function for parsing the file ===
def parse_file(filepath):
    with open(filepath, "r") as f:
        content = f.read()

    def extract_int(key):
        match = re.search(f"{key}:\\s+(\\d+)", content)
        if not match:
            print(f"[Warning] Field '{key}' not found at file: {filepath}")
            return 0
        return int(match.group(1))

    total_inst = extract_int("Total Instructions")
    total_br = extract_int("Total-Branches")
    ct = extract_int("Conditional-Taken-Branches")
    cnt = extract_int("Conditional-NotTaken-Branches")
    uncond = extract_int("Unconditional-Branches")
    calls = extract_int("Calls")
    rets = extract_int("Returns")

    # Percentages
    percent = lambda x: round(x / total_br * 100, 2) if total_br > 0 else 0

    return [total_inst, total_br, percent(ct), percent(cnt), percent(uncond), percent(calls), percent(rets)]

# === Parse all the files ===
for f in files:
    bench = f.split(".")[0]
    stats = parse_file(os.path.join(input_dir, f))
    rows.append([bench] + stats)

# === Save the CSV ===
with open(output_csv, "w", newline="") as csvfile:
    writer = csv.writer(csvfile)
    writer.writerow(header)
    writer.writerows(rows)

print(f"Saved at: {output_csv}")
