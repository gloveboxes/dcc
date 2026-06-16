#!/usr/bin/env bash
set -euo pipefail

# Performance capture script for benchmark apps
# Builds and runs apps in both peep and nopeep modes
# Outputs CSV: utc-timestamp,app,peep_ms,peep_size,nopeep_ms,nopeep_size

APPS="tstring sieve e tm ttt pihex mm"
BUILD_DIR="${BUILD_DIR:-build}"
OUTPUT_FILE="${OUTPUT_FILE:-perf_results.csv}"
EMULATOR="${EMULATOR:-ntvcm}"

mkdir -p "$BUILD_DIR"

# Temporary files to store peep and nopeep results
PEEP_RESULTS=$(mktemp)
NOPEEP_RESULTS=$(mktemp)
trap "rm -f '$PEEP_RESULTS' '$NOPEEP_RESULTS'" EXIT

stage_fixture_inputs() {
    for f in E.PAS E.COB E.FOR E.ADA E.BAS E.F EU.C; do
        if [ -f "tests/$f" ]; then
            cp -f "tests/$f" "${BUILD_DIR}/$f"
        elif [ -f "$f" ]; then
            cp -f "$f" "${BUILD_DIR}/$f"
        fi
    done
}

run_args() {
    case "$1" in
        ttt) echo "10" ;;
        *) echo "" ;;
    esac
}

clean_one() {
    app="$1"
    upper="$(printf '%s' "$app" | tr '[:lower:]' '[:upper:]')"
    rm -f "${BUILD_DIR}/${upper}.MAC" "${BUILD_DIR}/${upper}.REL" "${BUILD_DIR}/${upper}.PRN" "${BUILD_DIR}/${upper}.COM" \
          "${BUILD_DIR}/${app}.mac" "${BUILD_DIR}/${app}.rel" "${BUILD_DIR}/${app}.prn" "${BUILD_DIR}/${app}.com" "${BUILD_DIR}/${app}.COM"
}

run_mode() {
    mode="$1"
    results_file="$2"
    
    echo "Capturing $mode benchmarks..."
    
    > "$results_file"
    stage_fixture_inputs
    
    for app in $APPS; do
        upper="$(printf '%s' "$app" | tr '[:lower:]' '[:upper:]')"
        
        echo -n "  Building $app... "
        clean_one "$app"
        ./ma.sh "$app" "$mode" >/dev/null 2>&1
        echo "done"
        
        # Get binary size (in bytes)
        com_file="${BUILD_DIR}/${upper}.COM"
        if [ ! -f "$com_file" ]; then
            echo "    WARNING: $com_file not found, skipping"
            clean_one "$app"
            continue
        fi
        
        size=$(stat -f%z "$com_file" 2>/dev/null || stat -c%s "$com_file" 2>/dev/null || echo "0")
        
        echo -n "  Running $app... "
        
        # Run with timing and capture output
        args="$(run_args "$app")"
        
        # Use /usr/bin/time for millisecond precision
        if command -v /usr/bin/time >/dev/null; then
            # macOS/BSD time format: "real    0m42.123s"
            time_output=$( { /usr/bin/time -p sh -c "cd '$BUILD_DIR' && '$EMULATOR' '$upper' $args >/dev/null 2>&1" 2>&1; } | grep real | awk '{print $2}')
            # Convert real time to milliseconds (e.g., "0m42.123s" -> 42123 ms)
            ms=$(echo "$time_output" | sed 's/m//' | sed 's/s//' | awk -F. '{min=$1; print int(min*60000 + $2*1000)}')
        else
            # Fallback for systems without time command
            start_ms=$(date +%s%N | cut -b1-13)
            (cd "$BUILD_DIR" && $EMULATOR "$upper" $args >/dev/null 2>&1 || true)
            end_ms=$(date +%s%N | cut -b1-13)
            ms=$((end_ms - start_ms))
        fi
        
        echo "done (${ms}ms, ${size}B)"
        echo "$app:$ms:$size" >> "$results_file"
        
        clean_one "$app"
    done
}

# Capture both modes
run_mode peep "$PEEP_RESULTS"
echo ""
run_mode nopeep "$NOPEEP_RESULTS"
echo ""

# Merge results into final CSV
utc_timestamp=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
echo "utc-timestamp,app,peep_ms,peep_size,nopeep_ms,nopeep_size" > "$OUTPUT_FILE"

echo "Merging results..."
for app in $APPS; do
    peep_data=$(grep "^${app}:" "$PEEP_RESULTS" 2>/dev/null || echo "")
    nopeep_data=$(grep "^${app}:" "$NOPEEP_RESULTS" 2>/dev/null || echo "")
    
    if [ -z "$peep_data" ] || [ -z "$nopeep_data" ]; then
        echo "  WARNING: Missing data for $app"
        continue
    fi
    
    peep_ms=$(echo "$peep_data" | cut -d: -f2)
    peep_size=$(echo "$peep_data" | cut -d: -f3)
    nopeep_ms=$(echo "$nopeep_data" | cut -d: -f2)
    nopeep_size=$(echo "$nopeep_data" | cut -d: -f3)
    
    echo "$utc_timestamp,$app,$peep_ms,$peep_size,$nopeep_ms,$nopeep_size" >> "$OUTPUT_FILE"
done

echo ""
echo "Results saved to: $OUTPUT_FILE"
cat "$OUTPUT_FILE"
