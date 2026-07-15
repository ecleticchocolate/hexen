#!/usr/bin/env bash
# Torrent regression test suite — directory-walking runner.
#
# Every test is a real .t file under testsuite/. Each file declares its own
# expectation in a header directive so the file is fully self-describing:
#
#   //@ expect val N          compile+run, assert stdout contains "= N", exit 0
#   //@ expect err SUBSTR     assert compile fails (exit != 0), stderr has SUBSTR
#   //@ expect stdout         compile+run exit 0; each following
#   //@ | LINE                "//@ | LINE" must appear in stdout (in order)
#
# The //@ prefix is an ordinary Torrent line comment, so the compiler ignores
# it — the language stays unaware of the harness.
#
# Usage: ./test.sh [--aot] [-v] [path/to/torrent]
#   --aot  compile to ELF + link with gcc + run (proves AOT == JIT).
#   -v     print every PASS line (default: only failures + summary).
#   Filter: any non-flag arg that is NOT an executable is treated as a
#           substring filter on test names, e.g. ./test.sh enum/

set -euo pipefail
export ASAN_OPTIONS=detect_leaks=0

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SUITE="$SCRIPT_DIR/testsuite"

AOT_MODE=false
LLVM_MODE=false
VERBOSE=false
TORRENT=""
FILTER=""
for arg in "$@"; do
    case "$arg" in
        --aot) AOT_MODE=true ;;
        --llvm) LLVM_MODE=true ;;
        -v|--verbose) VERBOSE=true ;;
        *)
            if [[ -x "$arg" && ! -d "$arg" ]]; then
                TORRENT="$arg"
            else
                FILTER="$arg"
            fi
            ;;
    esac
done
TORRENT="${TORRENT:-$SCRIPT_DIR/torrent}"

if [[ ! -x "$TORRENT" || -d "$TORRENT" ]]; then
    echo "error: torrent binary not found or not executable: $TORRENT" >&2
    exit 1
fi
if [[ ! -d "$SUITE" ]]; then
    echo "error: testsuite directory not found: $SUITE" >&2
    exit 1
fi

AOT_SHIM=""
if $AOT_MODE; then
    echo "AOT mode: compile -c → gcc link (with shim) → run stdout"
    AOT_SHIM=$(mktemp /tmp/torrent_shim_XXXXXX.o)
    gcc -c -o "$AOT_SHIM" "$SCRIPT_DIR/aot_shim.c"
fi
if $LLVM_MODE; then
    echo "LLVM mode: compile -llvm → clang link (with shim) → run stdout"
fi

PASS=0
FAIL=0
ERRORS=()

pass() { $VERBOSE && echo "PASS  $1"; PASS=$((PASS+1)); }
fail() { echo "FAIL  $1  --  $2"; FAIL=$((FAIL+1)); ERRORS+=("$1"); }

# Compile+run a source FILE. Sets _STDOUT, _STDERR, _EXIT.
_run() {
    local file="$1"
    if $LLVM_MODE; then
        local ll bin cc_exit link_exit run_exit
        ll=$(mktemp /tmp/torrent_llvm_XXXXXX.ll)
        bin=$(mktemp /tmp/torrent_llvm_XXXXXX)
        "$TORRENT" -llvm "$ll" "$file" >/dev/null 2>/tmp/_torrent_stderr && cc_exit=0 || cc_exit=$?
        _STDERR=$(cat /tmp/_torrent_stderr)
        if [[ $cc_exit -ne 0 ]]; then
            rm -f "$ll" "$bin"; _STDOUT=""; _EXIT=$cc_exit; return
        fi
        # Hide clang warnings like "overriding the module target triple"
        clang -o "$bin" "$SCRIPT_DIR/aot_shim.c" "$ll" -Wno-override-module >/dev/null 2>/tmp/_torrent_stderr && link_exit=0 || link_exit=$?
        if [[ $link_exit -ne 0 ]]; then
            _STDERR="link error: $(cat /tmp/_torrent_stderr)"
            rm -f "$ll" "$bin"; _STDOUT=""; _EXIT=$link_exit; return
        fi
        _STDOUT=$("$bin" 2>/dev/null) && run_exit=0 || run_exit=$?
        rm -f "$ll" "$bin"; _EXIT=0
    elif $AOT_MODE; then
        local obj bin cc_exit link_exit run_exit
        obj=$(mktemp /tmp/torrent_aot_XXXXXX.o)
        bin=$(mktemp /tmp/torrent_aot_XXXXXX)
        "$TORRENT" -c -o "$obj" "$file" >/dev/null 2>/tmp/_torrent_stderr && cc_exit=0 || cc_exit=$?
        _STDERR=$(cat /tmp/_torrent_stderr)
        if [[ $cc_exit -ne 0 ]]; then
            rm -f "$obj" "$bin"; _STDOUT=""; _EXIT=$cc_exit; return
        fi
        gcc -o "$bin" "$AOT_SHIM" "$obj" 2>/tmp/_torrent_stderr && link_exit=0 || link_exit=$?
        if [[ $link_exit -ne 0 ]]; then
            _STDERR="link error: $(cat /tmp/_torrent_stderr)"
            rm -f "$obj" "$bin"; _STDOUT=""; _EXIT=$link_exit; return
        fi
        _STDOUT=$("$bin" 2>/dev/null) && run_exit=0 || run_exit=$?
        rm -f "$obj" "$bin"; _EXIT=0
    else
        _STDOUT=$("$TORRENT" "$file" 2>/tmp/_torrent_stderr) && _EXIT=0 || _EXIT=$?
        _STDERR=$(cat /tmp/_torrent_stderr)
    fi
}

# ─── walk the suite ──────────────────────────────────────────────────────────

while IFS= read -r file; do
    name="${file#"$SUITE"/}"; name="${name%.t}"
    [[ -n "$FILTER" && "$name" != *"$FILTER"* ]] && continue

    # parse the //@ header
    kind=""; arg=""; stdout_lines=()
    while IFS= read -r line; do
        case "$line" in
            '//@ expect val '*)    kind="val";    arg="${line#//@ expect val }" ;;
            '//@ expect err '*)    kind="err";    arg="${line#//@ expect err }" ;;
            '//@ expect stdout'*)  kind="stdout" ;;
            '//@ | '*)             stdout_lines+=("${line#//@ | }") ;;
            *) ;;  # first non-directive line: header done
        esac
        # stop scanning once we've left the header block
        [[ "$line" != //@* && -n "$kind" ]] && break
    done < "$file"

    if [[ -z "$kind" ]]; then
        fail "$name" "no //@ expect directive in file"
        continue
    fi

    _run "$file"

    case "$kind" in
        val)
            if [[ "$_EXIT" -ne 0 ]]; then
                fail "$name" "compiler exited $_EXIT; stderr: $_STDERR"
            elif echo "$_STDOUT" | grep -qF "= $arg"; then
                pass "$name"
            else
                fail "$name" "expected '= $arg', got: $_STDOUT"
            fi ;;
        err)
            if [[ "$_EXIT" -eq 0 ]]; then
                fail "$name" "expected compile error but succeeded; stdout: $_STDOUT"
            elif echo "$_STDERR" | grep -qF "$arg"; then
                pass "$name"
            else
                fail "$name" "expected stderr to contain '$arg', got: $_STDERR"
            fi ;;
        stdout)
            if [[ "$_EXIT" -ne 0 ]]; then
                fail "$name" "compiler exited $_EXIT; stderr: $_STDERR"
            else
                # Program output is stdout minus the trailing "= N" result line
                # (the harness result marker, not program output). Compare the
                # remaining lines exactly and in order against the //@ | block.
                got=$(printf '%s\n' "$_STDOUT" | grep -v '^= ')
                want=$(printf '%s\n' "${stdout_lines[@]}")
                if [[ "$got" == "$want" ]]; then
                    pass "$name"
                else
                    fail "$name" "stdout mismatch; want:[$want] got:[$got]"
                fi
            fi ;;
    esac
done < <(find "$SUITE" -name '*.t' | sort)

# ─── summary ─────────────────────────────────────────────────────────────────

echo ""
if [[ $FAIL -gt 0 ]]; then
    echo "Failed tests:"
    for n in "${ERRORS[@]}"; do echo "  $n"; done
    echo ""
    echo "$FAIL failed, $PASS passed."
    exit 1
else
    echo "All $PASS tests passed."
    exit 0
fi
