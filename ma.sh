#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: ./ma.sh name [peep|nopeep]"
    echo "examples: ./ma.sh e peep"
    echo "          ./ma.sh e nopeep"
}

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    usage
    exit 1
fi

name_arg="$1"
base="${name_arg%.*}"
lower_base="$(printf '%s' "$(basename "$base")" | tr '[:upper:]' '[:lower:]')"
upper_base="$(printf '%s' "$(basename "$base")" | tr '[:lower:]' '[:upper:]')"

mode="${2:-peep}"
mode_lc="$(printf '%s' "$mode" | tr '[:upper:]' '[:lower:]')"
case "$mode_lc" in
    peep|opt|optimized|o|-o|1|yes|true)
        use_peep=1
        ;;
    nopeep|nopeepopt|noopt|unopt|u|-u|0|no|false)
        use_peep=0
        ;;
    *)
        echo "unknown optimization mode: $mode" >&2
        usage
        exit 1
        ;;
esac

# Prefer tests/ first (where runall sources now live), but keep a root fallback.
# Also support CP/M-style uppercase/lowercase names on case-sensitive filesystems.
source_file=""
for cand in \
    "tests/${base}.c" \
    "tests/${base}.C" \
    "tests/${lower_base}.c" \
    "tests/${upper_base}.C" \
    "${base}.c" \
    "${base}.C" \
    "${lower_base}.c" \
    "${upper_base}.C"
do
    if [ -f "$cand" ]; then
        source_file="$cand"
        break
    fi
done
if [ -z "$source_file" ]; then
    echo "source file not found for: $name_arg" >&2
    exit 1
fi

DCC=${DCC:-dcc}
DCCPEEP=${DCCPEEP:-dccpeep}
DCCRTLSTRIP=${DCCRTLSTRIP:-dccrtlstrip}
NTVCM=${NTVCM:-ntvcm}
M80=${M80:-m80}
L80=${L80:-l80}

build_dir="build"
mkdir -p "$build_dir"

# ntvcm resolves COM command files from the working directory; when we run it
# in build/, stage the tool COM files there to avoid manual setup.
if [ -f "m80.com" ]; then
    cp -f "m80.com" "${build_dir}/m80.com"
fi
if [ -f "l80.com" ]; then
    cp -f "l80.com" "${build_dir}/l80.com"
fi

# M80/L80 on CP/M want uppercase filenames. Keep all generated CP/M-facing
# modules uppercase: FOO.MAC, FOO.REL, RTLMIN.MAC, RTLMIN.REL.
app_mac="${build_dir}/${upper_base}.MAC"
app_rel="${build_dir}/${upper_base}.REL"
app_com="${build_dir}/${upper_base}.COM"
peep_tmp="${build_dir}/_PEEPOUT.MAC"
rtl_src="${build_dir}/DCCRTL.MAC"
rtl_min="${build_dir}/RTLMIN.MAC"
root_rtl_src="DCCRTL.MAC"

if [ ! -f "$root_rtl_src" ]; then
    echo "runtime not found: $root_rtl_src" >&2
    exit 1
fi

# CRLF conversion helper for files consumed by M80.
to_crlf() {
    if command -v unix2dos >/dev/null 2>&1; then
        unix2dos "$1" >/dev/null 2>&1 || true
    else
        perl -0pi -e 's/\r?\n/\r\n/g' "$1"
    fi
}

# Detect optional compiler/runtime roots from source. Keep this conservative so
# ordinary programs do not pull in float or long printf code.
dcc_floatio=0
if grep -Eiq '%[-+ #0-9.*]*[fF]' "$source_file"; then
    dcc_floatio=1
fi
dcc_longio=0
if grep -Eiq '%[-+ #0-9.*]*l[duxXs]' "$source_file"; then
    dcc_longio=1
fi

# Enable the lightweight stack-overflow guard (-fstack-check) when the source
# opts in with a DCC_STACK_CHECK marker, or when DCC_FORCE_STACK_CHECK=1 is set
# in the environment (used by runall.sh --stack-check to guard the whole suite).
# Keeps every other binary free of the per-function guard call.
dcc_stackchk=""
if [ "${DCC_FORCE_STACK_CHECK:-0}" = "1" ] || grep -q 'DCC_STACK_CHECK' "$source_file"; then
    dcc_stackchk="-fstack-check"
fi

rm -f "$app_mac" "$app_rel" "$app_com" "${build_dir}/${upper_base}.PRN" \
    "$peep_tmp" "$rtl_src" "$rtl_min" "${build_dir}/RTLMIN.REL" "${build_dir}/RTLMIN.PRN"

# Compile directly to an uppercase .MAC file.
# Avoid empty-array expansion here: macOS ships Bash 3.2, where
# "${array[@]}" under set -u can fail when the array is empty.
# DCC_STACK_SIZE overrides the default 512-byte C stack reserve (handy for
# sweeping stack sizes under -fstack-check).
dcc_stack_size="${DCC_STACK_SIZE:-512}"
dcc_io_flags=""
if [ "$dcc_floatio" -eq 1 ]; then
    dcc_io_flags="$dcc_io_flags -ffloatio"
fi
if [ "$dcc_longio" -eq 1 ]; then
    dcc_io_flags="$dcc_io_flags -flongio"
fi
"$DCC" ${dcc_stackchk} $dcc_io_flags -stack "$dcc_stack_size" "$source_file" -o "$app_mac"

if [ "$use_peep" -eq 1 ]; then
    "$DCCPEEP" "$app_mac" "$peep_tmp"
    mv -f "$peep_tmp" "$app_mac"
fi

to_crlf "$app_mac"

# Assemble app. Explicit uppercase .MAC is important on Linux/macOS.
(
    cd "$build_dir"
    "$NTVCM" "$M80" "=${upper_base}.MAC" /X /O /Z /L
)

# Strip runtime using the final app .MAC, then assemble/link uppercase modules.
cp -f "$root_rtl_src" "$rtl_src"
to_crlf "$rtl_src"
strip_keep=""
if [ "$dcc_floatio" -eq 1 ]; then
    strip_keep="$strip_keep -k _pffio"
fi
if [ "$dcc_longio" -eq 1 ]; then
    strip_keep="$strip_keep -k _pflng"
fi
"$DCCRTLSTRIP" $strip_keep -r "$rtl_src" -o "$rtl_min" "$app_mac"
to_crlf "$rtl_min"

(
    cd "$build_dir"
    "$NTVCM" "$M80" "=RTLMIN.MAC" /X /O /Z
    "$NTVCM" "$L80" "/P:100,RTLMIN,${upper_base},${upper_base}/N/E"
)

# Convenience lowercase copy for host-side scripts/emulators that prefer it.
# On macOS's usual case-insensitive filesystems, TTT.COM and ttt.com may be
# the same file.  Avoid cp's "identical" error under set -e.
lower_com="${build_dir}/${lower_base}.com"
if [ -f "$app_com" ] && [ "$lower_base" != "$upper_base" ]; then
    if [ ! -e "$lower_com" ] || [ ! "$app_com" -ef "$lower_com" ]; then
        cp -f "$app_com" "$lower_com"
    fi
fi
