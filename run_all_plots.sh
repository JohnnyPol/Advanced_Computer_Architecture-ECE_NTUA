#!/bin/bash
# run_all_plots.sh

for file in outputs_predictions/refs/*.out; do
    echo "Processing $file"
    python3 plot_mpki_ipc.py "$file"
done
