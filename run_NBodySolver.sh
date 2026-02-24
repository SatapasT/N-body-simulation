#!/bin/bash
#SBATCH -J nbody_bench
#SBATCH -p shared
#SBATCH -c 16
#SBATCH --mem=8G
#SBATCH --time=0-02:00:00
#SBATCH --gres=tmp:16G
#SBATCH -o nbody_bench_%j.out
#SBATCH -e nbody_bench_%j.err

set -euo pipefail

module purge
module load gcc/12.2

echo "======================================"
echo "Job ID:     ${SLURM_JOB_ID}"
echo "Node(s):    ${SLURM_NODELIST}"
echo "CPUs:       ${SLURM_CPUS_PER_TASK}"
echo "Work dir:   $(pwd)"
echo "TMPDIR:     ${TMPDIR:-<none>}"
echo "SLURM_TMPDIR: ${SLURM_TMPDIR:-<none>}"
echo "Start time: $(date)"
echo "======================================"

export OMP_PROC_BIND=spread
export OMP_PLACES=cores

# ---------------- Scratch (node-local) ----------------
# Hamilton sometimes doesn't set SLURM_TMPDIR. Prefer SLURM_TMPDIR, else TMPDIR, else /tmp.
TMP_BASE="${SLURM_TMPDIR:-${TMPDIR:-/tmp}}"
SCRATCH_DIR="$(mktemp -d "${TMP_BASE}/nbody_${SLURM_JOB_ID}_XXXXXX")"

echo "Scratch base: ${TMP_BASE}"
echo "Scratch dir:  ${SCRATCH_DIR}"

cleanup() { rm -rf "${SCRATCH_DIR}" 2>/dev/null || true; }
trap cleanup EXIT

# ------------- Config -------------
BASELINE_EXE="./NBodySolver_ref"
FULL_EXE="./NBodySolver"

INPUTS=(
  "scenario1_stable.txt"
  "scenario2_unstable.txt"
  "collision_test_1000.txt"
  "big_random_512.txt"
  "input_5000.txt"
)

# ONLY 1, 8, 16 (as requested)
THREADS=(1 8 16)

# Report-quality numbers:
REPEATS=3      # median-of-3
WARMUP=1       # 1 warmup run (ignored)

# Copy-back control:
#   lite -> copy back only .pvd + first/last .vtp
#   tar  -> tar.gz whole paraview-output
#   none -> no paraview files copied back, only CSV+summary
COPY_BACK_MODE="lite"

# Save tiny paraview outputs ONLY for these configs:
SAVE_BASELINE_OUTPUTS=1      # baseline@1
SAVE_FULL_MAX_OUTPUTS=1      # full@max threads in THREADS array (here 16)

# Where to store results in $HOME
OUTROOT="bench_output"
mkdir -p "${OUTROOT}"

CSV="bench_${SLURM_JOB_ID}.csv"
SUMMARY="bench_summary_${SLURM_JOB_ID}.txt"
echo "scenario,variant,threads,wall_s" > "${CSV}"

declare -A TIME_SEC

# ------------- Build -------------
echo
echo ">>> Building baseline + full..."
make clean
make all

# sanity
for exe in "${BASELINE_EXE}" "${FULL_EXE}"; do
  if [[ ! -x "${exe}" ]]; then
    echo "ERROR: Missing executable: ${exe}"
    exit 1
  fi
done

# median helper
median_of_list () {
  # usage: median_of_list "1.2 2.3 0.9"
  local xs=($1)
  local n=${#xs[@]}
  if (( n == 0 )); then echo ""; return 0; fi
  IFS=$'\n' xs=($(printf "%s\n" "${xs[@]}" | sort -n))
  unset IFS
  if (( n % 2 == 1 )); then
    echo "${xs[$((n/2))]}"
  else
    awk -v a="${xs[$((n/2-1))]}" -v b="${xs[$((n/2))]}" 'BEGIN{printf "%.5f", (a+b)/2.0}'
  fi
}

# copy-back helper
copy_back_outputs () {
  local run_tmp_dir="$1"
  local outdir_home="$2"
  mkdir -p "${outdir_home}"

  case "${COPY_BACK_MODE}" in
    none)
      ;;
    lite)
      if compgen -G "${run_tmp_dir}/paraview-output/*.pvd" > /dev/null; then
        cp -f "${run_tmp_dir}/paraview-output/"*.pvd "${outdir_home}/" || true
      fi
      local first last
      first=$(ls -1 "${run_tmp_dir}/paraview-output/"result-*.vtp 2>/dev/null | head -n 1 || true)
      last=$(ls -1 "${run_tmp_dir}/paraview-output/"result-*.vtp 2>/dev/null | tail -n 1 || true)
      if [[ -n "${first}" ]]; then cp -f "${first}" "${outdir_home}/" || true; fi
      if [[ -n "${last}"  ]]; then cp -f "${last}"  "${outdir_home}/" || true; fi
      ;;
    tar)
      if [[ -d "${run_tmp_dir}/paraview-output" ]]; then
        (cd "${run_tmp_dir}" && tar -czf "paraview-output.tar.gz" "paraview-output") || true
        if [[ -f "${run_tmp_dir}/paraview-output.tar.gz" ]]; then
          cp -f "${run_tmp_dir}/paraview-output.tar.gz" "${outdir_home}/" || true
        fi
      fi
      ;;
    *)
      echo "WARN: Unknown COPY_BACK_MODE=${COPY_BACK_MODE}; skipping copy back."
      ;;
  esac
}

run_one () {
  local variant="$1"
  local exe="$2"
  local input="$3"
  local threads="$4"

  local tag="${variant}__t${threads}__${input%.txt}"
  local outdir_home="${OUTROOT}/${tag}"

  # Run in node-local scratch (avoids $HOME quota)
  local run_tmp_dir="${SCRATCH_DIR}/run_${tag}"
  rm -rf "${run_tmp_dir}"
  mkdir -p "${run_tmp_dir}"

  # stage input + executable
  cp -f "${input}" "${run_tmp_dir}/${input}"
  cp -f "${exe}" "${run_tmp_dir}/solver"

  (
    cd "${run_tmp_dir}"
    export OMP_NUM_THREADS="${threads}"

    # warmup (ignored)
    if (( WARMUP == 1 )); then
      ./solver "${input}" >/dev/null 2>/dev/null || true
      rm -rf paraview-output 2>/dev/null || true
    fi

    # repeats
    local times=()
    for r in $(seq 1 "${REPEATS}"); do
      local t
      t=$(/usr/bin/time -p ./solver "${input}" 2>&1 >/dev/null | awk '/^real /{print $2}')
      times+=("${t}")

      # remove output between repeats so timing is comparable
      rm -rf paraview-output 2>/dev/null || true
    done

    local med
    med=$(median_of_list "${times[*]}")
    echo "${med}"

    # Only generate/copy back outputs for baseline@1 and full@max thread in THREADS
    local save_outputs=0
    local max_thread="${THREADS[-1]}"

    if [[ "${COPY_BACK_MODE}" != "none" ]]; then
      if [[ "${variant}" == "baseline" && "${threads}" -eq 1 && "${SAVE_BASELINE_OUTPUTS}" -eq 1 ]]; then
        save_outputs=1
      fi
      if [[ "${variant}" == "full" && "${threads}" -eq "${max_thread}" && "${SAVE_FULL_MAX_OUTPUTS}" -eq 1 ]]; then
        save_outputs=1
      fi
    fi

    if (( save_outputs == 1 )); then
      ./solver "${input}" >/dev/null 2>/dev/null || true
      copy_back_outputs "${run_tmp_dir}" "${outdir_home}"
      rm -rf paraview-output 2>/dev/null || true
    fi
  )
}

echo
echo ">>> Running benchmarks..."
echo "  - baseline: threads fixed at 1"
echo "  - full: threads swept over: ${THREADS[*]}"
echo "  - repeats: ${REPEATS} (median recorded)"
echo "  - warmup:  ${WARMUP} (ignored)"
echo "  - outputs: COPY_BACK_MODE=${COPY_BACK_MODE} (only baseline@1 and full@${THREADS[-1]})"
echo

# main loop (don’t hard-fail job on one scenario; record NA)
for input in "${INPUTS[@]}"; do
  if [[ ! -f "${input}" ]]; then
    echo "ERROR: Missing input file: ${input}"
    continue
  fi

  echo "=============================="
  echo "Scenario: ${input}"
  echo "=============================="

  # baseline @ 1
  if t=$(run_one "baseline" "${BASELINE_EXE}" "${input}" 1); then
    TIME_SEC["${input}|baseline|1"]="${t}"
    echo "${input},baseline,1,${t}" >> "${CSV}"
    echo "baseline (1 thread): ${t}s"
  else
    TIME_SEC["${input}|baseline|1"]="NA"
    echo "${input},baseline,1,NA" >> "${CSV}"
    echo "baseline (1 thread): FAILED"
  fi

  # full @ threads (1,8,16)
  for th in "${THREADS[@]}"; do
    if (( th > SLURM_CPUS_PER_TASK )); then
      continue
    fi

    if t=$(run_one "full" "${FULL_EXE}" "${input}" "${th}"); then
      TIME_SEC["${input}|full|${th}"]="${t}"
      echo "${input},full,${th},${t}" >> "${CSV}"
      echo "full (${th} thread): ${t}s"
    else
      TIME_SEC["${input}|full|${th}"]="NA"
      echo "${input},full,${th},NA" >> "${CSV}"
      echo "full (${th} thread): FAILED"
    fi
  done

  echo
done

# summary table
{
  echo "======================================"
  echo "N-Body Benchmark Summary (baseline vs full)"
  echo "Job: ${SLURM_JOB_ID}"
  echo "Node(s): ${SLURM_NODELIST}"
  echo "Allocated cores: ${SLURM_CPUS_PER_TASK}"
  echo "Generated: $(date)"
  echo "Repeats (median): ${REPEATS}"
  echo "Scratch: ${SCRATCH_DIR}"
  echo "Outputs: ${OUTROOT}/ (COPY_BACK_MODE=${COPY_BACK_MODE})"
  echo "CSV: ${CSV}"
  echo "======================================"
  echo

  for input in "${INPUTS[@]}"; do
    base="${TIME_SEC["${input}|baseline|1"]}"

    echo "Scenario: ${input}"
    echo "  baseline@1: ${base}"
    echo
    printf "  %-6s  %-7s  %12s  %12s\n" "Var" "Threads" "Time(s)" "Speedup(vs base@1)"
    printf "  %-6s  %-7s  %12s  %12s\n" "------" "-------" "------------" "----------------"

    for th in "${THREADS[@]}"; do
      if (( th > SLURM_CPUS_PER_TASK )); then
        continue
      fi

      t="${TIME_SEC["${input}|full|${th}"]}"

      if [[ "${base}" == "NA" || "${t}" == "NA" || -z "${base}" || -z "${t}" ]]; then
        printf "  %-6s  %-7d  %12s  %12s\n" "full" "${th}" "${t:-NA}" "NA"
      else
        spd=$(awk -v b="${base}" -v x="${t}" 'BEGIN{ if (x>0) printf("%.2fx", b/x); else print("inf"); }')
        printf "  %-6s  %-7d  %12.3f  %12s\n" "full" "${th}" "${t}" "${spd}"
      fi
    done

    echo
    echo "--------------------------------------"
    echo
  done
} | tee "${SUMMARY}"

echo "Done."
echo "End time: $(date)"
echo "Outputs:"
echo "  - ${CSV}"
echo "  - ${SUMMARY}"
echo "  - ${OUTROOT}/"