#!/usr/bin/env bash
# Compare failing render tests with Google Chrome and optionally approve them.
# See README.md for details.
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd "$script_dir/.." && pwd)

tests_dir=""
work="$script_dir/work"
jobs=8
apply=0
capture=1
chrome=""
filters=()

usage() {
	cat <<EOF
Usage: $0 [options]

Compares every <test>-FAILED.png under a test tree with Google Chrome and
writes results/reports. With --apply, tests classified "approve" are renamed
from <test>-FAILED.png to <test>.png (the golden image).

Options:
  --tests-dir DIR   Tree containing *-FAILED.png (default: <repo>/render)
  --css-grid        Shorthand for --tests-dir <repo>/render/css-grid plus
                    --skip-refs --skip-dir parsing --skip-dir animation
                    --skip-dir reference
  --work DIR        Work directory (default: $script_dir/work)
  --jobs N          Concurrent Chrome pages (default 8)
  --apply           Rename approved FAILED images to goldens (default: dry run)
  --no-capture      Reuse screenshots from a previous run; only compare
  --chrome PATH     Chrome executable (auto-detected by default)
  -h, --help        Show this help

Outputs in the work directory:
  jobs.tsv, chrome/..., results.csv, approved.txt, report/review.html,
  report/rejected.html, capture_errors.txt

Example:
  # 1. run the render suite first (produces *-FAILED.png)
  ./run_render_tests.sh /path/to/litehtml 14
  # 2. triage the css-grid failures and approve matching ones
  ./check-with-chrome/run.sh --css-grid --work /tmp/css-grid-check --apply
EOF
}

while [[ $# -gt 0 ]]; do
	case "$1" in
		--tests-dir) tests_dir=${2:?}; shift 2 ;;
		--css-grid)
			tests_dir="$repo/render/css-grid"
			filters+=(--skip-refs --skip-dir parsing --skip-dir animation --skip-dir reference)
			shift
			;;
		--work) work=${2:?}; shift 2 ;;
		--jobs) jobs=${2:?}; shift 2 ;;
		--apply) apply=1; shift ;;
		--no-capture) capture=0; shift ;;
		--chrome) chrome=${2:?}; shift 2 ;;
		-h | --help) usage; exit 0 ;;
		*)
			echo "unknown option: $1" >&2
			usage
			exit 1
			;;
	esac
done

[[ -n "$tests_dir" ]] || tests_dir="$repo/render"
[[ -d "$tests_dir" ]] || {
	echo "not a directory: $tests_dir" >&2
	exit 1
}
mkdir -p "$work"

python3 "$script_dir/scripts/gen_fonts_conf.py" --repo "$repo" --out "$work"
python3 "$script_dir/scripts/make_jobs.py" --tests-dir "$tests_dir" --work "$work" "${filters[@]}"

if [[ $capture -eq 1 ]]; then
	chrome_args=()
	[[ -n "$chrome" ]] && chrome_args=(--chrome "$chrome")
	python3 "$script_dir/scripts/chrome_capture.py" \
		--list "$work/jobs.tsv" \
		--errors "$work/capture_errors.txt" \
		--fontconfig "$work/fonts.conf" \
		--jobs "$jobs" \
		--profile "$work/chrome-profile" \
		--home "$work/chrome-home" \
		"${chrome_args[@]}"
fi

python3 "$script_dir/scripts/compare.py" \
	--list "$work/jobs.tsv" \
	--csv "$work/results.csv" \
	--report-dir "$work/report" \
	--make-diffs

if [[ $apply -eq 1 ]]; then
	python3 "$script_dir/scripts/approve.py" --csv "$work/results.csv" \
		--tests-dir "$tests_dir" --list-out "$work/approved.txt"
	echo "Renamed approved goldens under $tests_dir"
else
	python3 "$script_dir/scripts/approve.py" --csv "$work/results.csv" \
		--tests-dir "$tests_dir" --list-out "$work/approved.txt" --dry-run
	echo "Dry run: nothing renamed. Re-run with --apply to approve."
fi

echo
echo "Results:  $work/results.csv"
echo "Review:   $work/report/review.html"
echo "Rejected: $work/report/rejected.html"
echo "Approved: $work/approved.txt"
