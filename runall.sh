#!/usr/bin/env bash
set -uo pipefail

# Optional first argument: emulator command. Defaults to ntvcm.
EMULATOR=${1:-ntvcm}
BUILD_DIR=${BUILD_DIR:-build}

mkdir -p "$BUILD_DIR"

APPLIST="sieve e ttt tstruct trw tstr tbug tprintf ts tcmp tunary tlong \
         tpi mm tm tfio wumpus triangle fileops nqueens fact tsetjmp \
         tenum tunion tgoto tarray tchess targs tstdc tvariad tsprintf tscanf \
         tdowhile tmuldiv tbdos tdirent tc89core tc89uac tc89init tc89fp \
         tc89ptr tc89size tc89decl tc89qual tc89comp tc89swjt tc89bit \
         tc89pp tc89fnty tc89flt tc89fltc tc89flta tc89fptr tc89fs tc89fcmp \
         tc89fcnv tc89fadd tc89fmul tc89fdiv tc89ffio tc89flng tc89fmat \
         ttrig tlog tphi tap cpmenumd tbits tfo pihex tstrify tlcont primes \
         tpreproc trwold tlimits spsmash tcrcfix trtl2 tsyntax tstr2 tstr3 tstring \
         tlongaud tlongreg tlongopt tctxops tppreg tinitreg ttypesr ttype2 tdecinit tmalloch \
         tallocx tstdlib tioerr tqsort tbsearch trw2 terrno tpostfld tswitch tppifcom tpostidx \
         tpostut tbug2 tlongsub treg tret tstructv tstructi tstructp tstri2 \
         tunion2 tbitfld tcnstfld tpromo tkandr tc89ini2 tdecl tctype tifcom \
         tptrdiff tmulpow2 toffset tc89fini tmod3216 tpromo2 tunaryp tstfield \
         pint cobint forint bint fint cint adaint tstretst tportio tlongidx tforsco \
         tforblk tcmt99 tc99scpe tctxflt tmathf tstrconv tfarrsub"

run_args() {
    case "$1" in
        ttt) echo "10" ;;
        pint) echo "e.pas" ;;
        cobint) echo "e.cob" ;;
        forint) echo "e.for" ;;
        adaint) echo "e.ada" ;;
        bint) echo "e.bas" ;;
        fint) echo "e.f" ;;
        cint) echo "eu.c" ;;
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
    rm -f "${BUILD_DIR}/${upper}.MAC" "${BUILD_DIR}/${upper}.REL" "${BUILD_DIR}/${upper}.PRN" "${BUILD_DIR}/${upper}.COM" \
          "${BUILD_DIR}/${app}.mac" "${BUILD_DIR}/${app}.rel" "${BUILD_DIR}/${app}.prn" "${BUILD_DIR}/${app}.com" "${BUILD_DIR}/${app}.COM" \
          "${BUILD_DIR}/DCCRTL.MAC" "${BUILD_DIR}/RTLMIN.MAC" "${BUILD_DIR}/RTLMIN.REL" "${BUILD_DIR}/RTLMIN.PRN" "${BUILD_DIR}/_PEEPOUT.MAC"
}

stage_fixture_inputs() {
    # Some tests read CP/M data files (E.*). Since test binaries run from
    # build/, ensure those files are present there. Prefer tests/ and keep
    # root fallback for compatibility.
    for f in E.PAS E.COB E.FOR E.ADA E.BAS E.F EU.C; do
        if [ -f "tests/$f" ]; then
            cp -f "tests/$f" "${BUILD_DIR}/$f"
        elif [ -f "$f" ]; then
            cp -f "$f" "${BUILD_DIR}/$f"
        fi
    done
}

run_set() {
    mode="$1"       # peep or nopeep, passed directly to ma.sh
    outfile="$2"

    : > "$outfile"
    stage_fixture_inputs

    for app in $APPLIST; do
        upper="$(printf '%s' "$app" | tr '[:lower:]' '[:upper:]')"
        echo "test $app"
        echo "test $app" >> "$outfile"

        clean_one "$app"
        ./ma.sh "$app" "$mode" >/dev/null

        args="$(run_args "$app")"
        if [ -n "$args" ]; then
            # shellcheck disable=SC2086
            (cd "$BUILD_DIR" && "$EMULATOR" "$upper" $args) >> "$outfile"
        else
            (cd "$BUILD_DIR" && "$EMULATOR" "$upper") >> "$outfile"
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
