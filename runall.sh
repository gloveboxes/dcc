#!/usr/bin/env bash
set -uo pipefail

# Optional first argument: emulator command. Defaults to ntvcm.
EMULATOR=${1:-ntvcm}

APPLIST="sieve e ttt tstruct trw tstr tbug tprintf ts tcmp tunary tlong \
         tpi mm tm tfio wumpus triangle fileops nqueens fact tsetjmp \
         tenum tunion tgoto tarray tchess targs tstdc tvariad tsprintf tscanf \
         tdowhile tmuldiv tbdos tdirent tc89core tc89uac tc89init tc89fp \
         tc89ptr tc89size tc89decl tc89qual tc89comp tc89swjt tc89bit \
         tc89pp tc89fnty tc89flt tc89fltc tc89flta tc89fptr tc89fs tc89fcmp \
         tc89fcnv tc89fadd tc89fmul tc89fdiv tc89ffio tc89flng tc89fmat \
         ttrig tlog tphi tap cpmenumd tbits tfo pihex tstrify tlcont primes \
         tpreproc trwold tlimits spsmash tcrcfix trtl2 tsyntax tstr2 tstr3 \
         tlongaud tlongreg tppreg tinitreg ttypesr ttype2 tdecinit tmalloch \
         tallocx tstdlib \
         tpostut tbug2 tlongsub treg tret tstructv tstructi tstructp tstri2 \
         tunion2 tbitfld tcnstfld tpromo tkandr tc89ini2 tdecl tctype \
         tptrdiff tmulpow2 toffset tc89fini tmod3216 tpromo2 tunaryp"

run_args() {
    case "$1" in
        ttt) echo "10" ;;
        wumpus|tchess) echo "-c" ;;
        targs) echo "a bb ccc dddd eeeee" ;;
        *) echo "" ;;
    esac
}

to_lf() {
    if command -v dos2unix >/dev/null 2>&1; then
        dos2unix "$1" >/dev/null 2>&1 || true
    else
        perl -0pi -e 's/\r\n/\n/g' "$1"
    fi
}

clean_one() {
    app="$1"
    upper="$(printf '%s' "$app" | tr '[:lower:]' '[:upper:]')"
    rm -f "${upper}.MAC" "${upper}.REL" "${upper}.PRN" "${upper}.COM" \
          "${app}.mac" "${app}.rel" "${app}.prn" "${app}.com" "${app}.COM" \
          RTLMIN.MAC RTLMIN.REL RTLMIN.PRN _PEEPOUT.MAC
}

run_set() {
    mode="$1"       # peep or nopeep, passed directly to ma.sh
    outfile="$2"

    : > "$outfile"

    for app in $APPLIST; do
        upper="$(printf '%s' "$app" | tr '[:lower:]' '[:upper:]')"
        echo "test $app"
        echo "test $app" >> "$outfile"

        clean_one "$app"
        ./ma.sh "$app" "$mode" >/dev/null

        args="$(run_args "$app")"
        if [ -n "$args" ]; then
            # shellcheck disable=SC2086
            "$EMULATOR" "$upper" $args >> "$outfile"
        else
            "$EMULATOR" "$upper" >> "$outfile"
        fi

        clean_one "$app"
    done
}

echo "--- optimized ---"
run_set peep test_dcc.txt

echo "--- unoptimized ---"
run_set nopeep test_dccu.txt

to_lf test_dcc.txt
to_lf test_dccu.txt

set +e
diff baseline_test_dcc.txt test_dcc.txt
diff baseline_test_dcc.txt test_dccu.txt
