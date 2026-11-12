#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

PROBE="$SRC/mayhem/build/test/kat_probe"
if [ ! -x "$PROBE" ]; then
  echo "kat_probe missing: mayhem/build.sh should have built it" >&2
  emit_ctrf "owntone-kat" 0 1 0
  exit 1
fi

passed=0
failed=0

# check <label> <expected stdout> <probe args...>: the probe prints the parse result for a known input.
check() {
  local label="$1" expected="$2" actual
  shift 2
  actual="$("$PROBE" "$@" 2>/dev/null)"
  if [ "$actual" = "$expected" ]; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1))
    echo "FAIL $label: expected [$expected] got [$actual]" >&2
  fi
}

# DAAP query -> SQL (httpd_daap/dmap_common)
check daap-single       "0|f.album_artist = 'Beatles'" daap "'daap.songartist:Beatles'"
check daap-and          "0|f.album_artist = 'Beatles' AND f.album = 'Abbey Road'" daap "'daap.songartist:Beatles'+'daap.songalbum:Abbey Road'"
check daap-or-not       "0|(f.album_artist = 'Beatles' OR f.album_artist <> 'Stones')" daap "('daap.songartist:Beatles','daap.songartist!:Stones')"
check daap-wildcard     "0|f.title LIKE '%love%'" daap "'dmap.itemname:*love*'"
check daap-int          "0|f.year = 1965" daap "'daap.songyear:1965'"
check daap-unknown-key  "-1|" daap "'nonsense.key:1'"
check daap-garbage      "-1|" daap "garbage"

# RSP (Roku SoundBridge) query -> SQL
check rsp-like          "0|f.artist LIKE '%Bea%' AND f.track = 3" rsp 'artist includes "Bea" and track = 3'
check rsp-eq            "0|f.title = 'Yesterday'" rsp 'title = "Yesterday"'
check rsp-incomplete    "-1|" rsp 'artist ='

# MPD filter expressions -> SQL fragments: ret|where|order|group|limit|offset
check mpd-find-sort     "0|f.artist = 'X'|f.title ASC||0|0" mpd 'find (artist == "X") sort title'
check mpd-group         "0|||f.artist , f.album|0|0" mpd 'list album group artist'
check mpd-int           "0|f.track >= 3|||0|0" mpd 'find (track >= 3)'
check mpd-incomplete    "-1||||0|0" mpd 'find (artist =='

# Smart playlist: ret|title|where|having|order|limit
check smartpl-random    "0|Fav|genre LIKE '%Rock%' OR rating > 60||random()|0" smartpl '"Fav" { genre includes "Rock" or rating > 60 order by random }'
check smartpl-media     "0|Pods|media_kind = 4 AND title NOT LIKE '%x%'||title ASC|0" smartpl '"Pods" { media_kind is podcast and not title includes "x" order by title asc }'
check smartpl-limit     "0|My Pl|artist = 'Beatles' AND year > 1965||year DESC|10" smartpl '"My Pl" { artist is "Beatles" and year > 1965 order by year desc limit 10 }'
check smartpl-invalid   "-1|||||0" smartpl '"bad" { artist is }'

# AirPlay 2 pairing TLV8 codec: parse then re-encode, hex in/out
check tlv-state-pk      "0|6:01|3:0a0b0c|fmt0:06010103030a0b0c" tlv 06010103030a0b0c
check tlv-ident-enc     "0|6:03|1:75736572|5:deadbeef|fmt0:0601030104757365720504deadbeef" tlv 0601030104757365720504deadbeef
check tlv-truncated     "-2" tlv 0601010103

# XML wrapper over libxml2
check xml-nested        "deep" xml '<a><b id="1">hi</b><b id="2">yo</b><c><d>deep</d></c></a>' a/c/d
check xml-first-sibling "1" xml '<a><b>1</b><b>2</b></a>' a/b
check xml-malformed     "parse-error" xml '<a><b>' a/b
check xml-missing-path  "(null)" xml '<a><b>x</b></a>' a/z

emit_ctrf "owntone-kat" "$passed" "$failed" 0
