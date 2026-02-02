#!/usr/bin/env bash
set -euo pipefail

# ---------- CONFIG ----------
BIN="build-release-timing/gtfs2rdf"
DATASET="../data/öv_de_shapes.zip"
# DATASET="mock_data/romania_mock.zip"

FORMAT="ttl"                  # ttl|nt
INTERVAL="0.5"                # psrecord sampling interval
WSL_BUDGET_MB=7000            # rough budget; used only to skip extreme combos
HEADROOM_MB=800              # safety headroom to avoid OOM (tune if needed)

# Small grid that fits ~1.5h with ~4min/run => aim ~20 runs
READ_SIZES_MB=(20 35 50 65 80 100)
WRITE_SIZES_MB=(20 35 50 65 80 100)   # if you want same as read, set equal arrays or couple below
STORAGE_SIZES_MB=(500 1000 1500 2000)

# If you want to couple read=write (often reasonable), set COUPLE_RW=1
COUPLE_RW=1

BASE_OUT_DIR="grid_runs_$(date +%Y%m%d_%H%M%S)"
TMP_OUT_DIR="${BASE_OUT_DIR}/_tmp_output"  # output file will be created here then deleted
mkdir -p "$BASE_OUT_DIR" "$TMP_OUT_DIR"

# ---------- HELPERS ----------
need_cmd() { command -v "$1" >/dev/null 2>&1 || { echo "Missing command: $1" >&2; exit 1; }; }
need_cmd psrecord
need_cmd sudo
need_cmd sync
need_cmd free
need_cmd rm
need_cmd date
need_cmd mkdir

stem="$(basename "$DATASET" .zip)"
ext=".$FORMAT"
OUT_FILE="${TMP_OUT_DIR}/${stem}${ext}"

drop_caches() {
  echo "----- drop_caches @ $(date) -----"
  free
  sync
  # requires root:
  sudo sh -c 'echo 3 >/proc/sys/vm/drop_caches'
  free
}

cleanup() {
  # stop sudo keepalive if running
  if [[ -n "${SUDO_KEEPALIVE_PID:-}" ]]; then
    kill "${SUDO_KEEPALIVE_PID}" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

# ---------- SUDO SETUP ----------
echo "Will ask for sudo once (needed for drop_caches)."
sudo -v

# keep sudo alive for long runs (refresh every 60s)
( while true; do sudo -v; sleep 60; done ) &
SUDO_KEEPALIVE_PID=$!

# ---------- GRID RUN ----------
RUN_IDX=0
TOTAL_START="$(date +%s)"

run_one() {
  local r="$1" w="$2" s="$3"

  # rough skip if the sum is too close to budget (very conservative heuristic)
  local rough=$(( r + w + s + HEADROOM_MB ))
  if (( rough > WSL_BUDGET_MB )); then
    echo "SKIP r=${r} w=${w} s=${s} (rough ${rough}MB > budget ${WSL_BUDGET_MB}MB)"
    return 0
  fi

  RUN_IDX=$((RUN_IDX+1))
  local tag="r${r}_w${w}_s${s}"
  local run_dir="${BASE_OUT_DIR}/run_${RUN_IDX}_${tag}"
  mkdir -p "$run_dir"

  echo "==============================="
  echo "RUN #${RUN_IDX}  ${tag}"
  echo "run_dir: ${run_dir}"
  echo "==============================="

  # always remove previous output
  rm -f "$OUT_FILE"

  # pre-run cache drop
  drop_caches | tee "${run_dir}/dropcaches_before.txt"

  # build command
  local cmd="${BIN} \"${DATASET}\" --format ${FORMAT} --output \"${TMP_OUT_DIR}\" --overwrite \
--read-buffer-size ${r} --write-buffer-size ${w} --storage-buffer-size ${s}"

  echo "$cmd" > "${run_dir}/cmd.txt"

  # run with psrecord; redirect tool stdout/stderr into file
  # NOTE: redirection must happen INSIDE the command string psrecord runs:
  psrecord "bash -lc '$cmd > \"${run_dir}/stdout.txt\" 2>&1'" \
    --interval "${INTERVAL}" \
    --include-children \
    --plot "${run_dir}/ram.png" \
    --log "${run_dir}/ram.log" \
    > "${run_dir}/psrecord_stdout.txt" 2>&1

  # delete output after run (keeps disk usage low)
  rm -f "$OUT_FILE"

  # post-run cache drop (as you requested)
  drop_caches | tee "${run_dir}/dropcaches_after.txt"
}

if (( COUPLE_RW == 1 )); then
  for rw in "${READ_SIZES_MB[@]}"; do
    for s in "${STORAGE_SIZES_MB[@]}"; do
      run_one "$rw" "$rw" "$s"
    done
  done
else
  for r in "${READ_SIZES_MB[@]}"; do
    for w in "${WRITE_SIZES_MB[@]}"; do
      for s in "${STORAGE_SIZES_MB[@]}"; do
        run_one "$r" "$w" "$s"
      done
    done
  done
fi

TOTAL_END="$(date +%s)"
echo "Done. Runs: ${RUN_IDX}. Total seconds: $((TOTAL_END - TOTAL_START))"
echo "All results in: ${BASE_OUT_DIR}/"
