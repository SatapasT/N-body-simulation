#!/bin/bash
set -euo pipefail

FILES=(
  scenario2_dt_0p001.txt
  scenario2_dt_0p0005.txt
  scenario2_dt_0p0001.txt
  scenario2_dt_0p00005.txt
)

for f in "${FILES[@]}"; do
  echo "======================================"
  echo "Running: $f"
  echo "======================================"

  if [[ ! -s "$f" ]]; then
    echo "ERROR: missing or empty file: $f"
    continue
  fi

  rm -rf paraview-output
  ./NBodySolver "$f"

  if [[ -f paraview-output/result-5000.vtp ]]; then
    echo "--- result-5000.vtp for $f ---"
    cat paraview-output/result-5000.vtp
    echo ""
  else
    echo "ERROR: result-5000.vtp not found for $f"
    echo "Files produced:"
    ls -1 paraview-output | head -n 20 || true
  fi
done