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
echo "Job ID:        ${SLURM_JOB_ID}"
echo "Node(s):       ${SLURM_NODELIST}"
echo "CPUs:          ${SLURM_CPUS_PER_TASK}"
echo "Work dir:      $(pwd)"
echo "TMPDIR:        ${TMPDIR:-<none>}"
echo "SLURM_TMPDIR:  ${SLURM_TMPDIR:-<none>}"
echo "Start time:    $(date)"
echo "======================================"

export OMP_PROC_BIND=spread
export OMP_PLACES=cores

TMP_BASE="${SLURM_TMPDIR:-${TMPDIR:-/tmp}}"
SCRATCH_DIR="$(mktemp -d "${TMP_BASE}/nbody_${SLURM_JOB_ID}_XXXXXX")"

echo "Scratch base:  ${TMP_BASE}"
echo "Scratch dir:   ${SCRATCH_DIR}"

cleanup() { rm -rf "${SCRATCH_DIR}" 2>/dev/null || true; }
trap cleanup EXIT

SCALAR_EXE="./NBodySolver_scalar"
REF_EXE="./NBodySolver_ref"
FULL_EXE="./NBodySolver"

# NOTE: input_5000 removed
INPUTS=(
  "scenario1_stable.txt"
  "scenario2_unstable.txt"
  "collision_test_1000.txt"
  "big_random_512.txt"
)

THREADS=(1 2 4 8 16)

REPEATS=3
WARMUP=1

COPY_BACK_MODE="lite"
SAVE_REF_OUTPUTS=1
SAVE_FULL_MAX_OUTPUTS=1

OUTROOT="bench_output"
mkdir -p "${OUTROOT}"

CSV="bench_${SLURM_JOB_ID}.csv"
SUMMARY="bench_summary_${SLURM_JOB_ID}.txt"
echo "scenario,variant,threads,wall_s" > "${CSV}"

declare -A TIME_SEC

echo
echo ">>> Building scalar + ref + full..."
make clean
make all

for exe in "${SCALAR_EXE}" "${REF_EXE}" "${FULL_EXE}"; do
  if [[ ! -x "${exe}" ]]; then
    echo "ERROR: Missing executable: ${exe}"
    exit 1
  fi
done

median_of_list () {
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

copy_back_outputs () {
  local run_tmp_dir="$1"
  local outdir_home="$2"
  mkdir -p "${outdir_home}"

  case "${COPY_BACK_MODE}" in
    none) ;;
    lite)
      if compgen -G "${run_tmp_dir}/paraview-output/*.pvd" > /dev/null; then
        cp -f "${run_tmp_dir}/paraview-output/"*.pvd "${outdir_home}/" || true
      fi
      local first last
      first=$(ls -1 "${run_tmp_dir}/paraview-output/"result-*.vtp 2>/dev/null | head -n 1 || true)
      last=$(ls -1 "${run_tmp_dir}/paraview-output/"result-*.vtp 2>/dev/null | tail -n 1 || true)
      [[ -n "${first}" ]] && cp -f "${first}" "${outdir_home}/" || true
      [[ -n "${last}"  ]] && cp -f "${last}"  "${outdir_home}/" || true
      ;;
    tar)
      if [[ -d "${run_tmp_dir}/paraview-output" ]]; then
        (cd "${run_tmp_dir}" && tar -czf "paraview-output.tar.gz" "paraview-output") || true
        [[ -f "${run_tmp_dir}/paraview-output.tar.gz" ]] && cp -f "${run_tmp_dir}/paraview-output.tar.gz" "${outdir_home}/" || true
      fi
      ;;
    *) echo "WARN: Unknown COPY_BACK_MODE=${COPY_BACK_MODE}; skipping copy back." ;;
  esac
}

run_one () {
  local variant="$1"
  local exe="$2"
  local input="$3"
  local threads="$4"

  local tag="${variant}__t${threads}__${input%.txt}"
  local outdir_home="${OUTROOT}/${tag}"

  local run_tmp_dir="${SCRATCH_DIR}/run_${tag}"
  rm -rf "${run_tmp_dir}"
  mkdir -p "${run_tmp_dir}"

  cp -f "${input}" "${run_tmp_dir}/${input}"
  cp -f "${exe}"   "${run_tmp_dir}/solver"

  (
    cd "${run_tmp_dir}"
    export OMP_NUM_THREADS="${threads}"

    if (( WARMUP == 1 )); then
      ./solver "${input}" >/dev/null 2>/dev/null || true
      rm -rf paraview-output 2>/dev/null || true
    fi

    local times=()
    for r in $(seq 1 "${REPEATS}"); do
      local t
      t=$(/usr/bin/time -p ./solver "${input}" 2>&1 >/dev/null | awk '/^real /{print $2}')
      times+=("${t}")
      rm -rf paraview-output 2>/dev/null || true
    done

    local med
    med=$(median_of_list "${times[*]}")
    echo "${med}"

    # save outputs only for ref@1 and full@max threads (optional)
    local save_outputs=0
    local max_thread="${THREADS[-1]}"

    if [[ "${COPY_BACK_MODE}" != "none" ]]; then
      if [[ "${variant}" == "ref" && "${threads}" -eq 1 && "${SAVE_REF_OUTPUTS}" -eq 1 ]]; then
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
echo "  - scalar: 1 thread (no vectorisation)"
echo "  - ref:    1 thread (vectorised, no OpenMP)"
echo "  - full:   threads swept over: ${THREADS[*]} (OpenMP + vectorised)"
echo "  - repeats: ${REPEATS} (median recorded)"
echo "  - warmup:  ${WARMUP} (ignored)"
echo

for input in "${INPUTS[@]}"; do
  [[ -f "${input}" ]] || { echo "ERROR: Missing input file: ${input}"; continue; }

  echo "=============================="
  echo "Scenario: ${input}"
  echo "=============================="

  if t=$(run_one "scalar" "${SCALAR_EXE}" "${input}" 1); then
    TIME_SEC["${input}|scalar|1"]="${t}"
    echo "${input},scalar,1,${t}" >> "${CSV}"
    echo "scalar (1 thread): ${t}s"
  else
    TIME_SEC["${input}|scalar|1"]="NA"
    echo "${input},scalar,1,NA" >> "${CSV}"
    echo "scalar (1 thread): FAILED"
  fi

  if t=$(run_one "ref" "${REF_EXE}" "${input}" 1); then
    TIME_SEC["${input}|ref|1"]="${t}"
    echo "${input},ref,1,${t}" >> "${CSV}"
    echo "ref (1 thread): ${t}s"
  else
    TIME_SEC["${input}|ref|1"]="NA"
    echo "${input},ref,1,NA" >> "${CSV}"
    echo "ref (1 thread): FAILED"
  fi

  for th in "${THREADS[@]}"; do
    (( th <= SLURM_CPUS_PER_TASK )) || continue
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

{
  echo "======================================"
  echo "N-Body Benchmark Summary (scalar vs ref vs full)"
  echo "Job: ${SLURM_JOB_ID}"
  echo "Node(s): ${SLURM_NODELIST}"
  echo "Allocated cores: ${SLURM_CPUS_PER_TASK}"
  echo "Generated: $(date)"
  echo "Repeats (median): ${REPEATS}"
  echo "CSV: ${CSV}"
  echo "======================================"
  echo

  for input in "${INPUTS[@]}"; do
    echo "Scenario: ${input}"
    echo "  scalar@1: ${TIME_SEC["${input}|scalar|1"]:-NA}"
    echo "  ref@1:    ${TIME_SEC["${input}|ref|1"]:-NA}"
    echo
    printf "  %-6s  %-7s  %12s\n" "Var" "Threads" "Time(s)"
    printf "  %-6s  %-7s  %12s\n" "------" "-------" "------------"

    for th in "${THREADS[@]}"; do
      (( th <= SLURM_CPUS_PER_TASK )) || continue
      t="${TIME_SEC["${input}|full|${th}"]}"
      printf "  %-6s  %-7d  %12s\n" "full" "${th}" "${t:-NA}"
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