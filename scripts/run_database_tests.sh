#!/bin/bash

set -euo pipefail

# ─── Config ───────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
INIT_SCRIPT="$SCRIPT_DIR/init_databases.sh"
TEST_BINARY="$PROJECT_ROOT/build/libs/unit_test/test_database_client"

# ─── Helpers ──────────────────────────────────────────────────────────────────
log_info()    { echo "[INFO]  $*"; }
log_success() { echo "[OK]    $*"; }
log_error()   { echo "[ERROR] $*" >&2; }

die() {
    log_error "$*"
    exit 1
}

cleanup() {
    log_info "Stopping QuestDB..."
    if command -v questdb &>/dev/null; then
        questdb stop 2>/dev/null || true
        log_success "QuestDB stopped."
    else
        log_info "questdb command not found — skipping stop."
    fi
}

# Always run cleanup on exit, whether the tests pass or fail.
trap cleanup EXIT

# ─── Pre-flight checks ───────────────────────────────────────────────────────
if [[ ! -f "$INIT_SCRIPT" ]]; then
    die "Database init script not found: $INIT_SCRIPT"
fi

if [[ ! -f "$TEST_BINARY" ]]; then
    # Try the other common build directory
    ALT_BINARY="$PROJECT_ROOT/build-test/libs/unit_test/test_database_client"
    if [[ -f "$ALT_BINARY" ]]; then
        TEST_BINARY="$ALT_BINARY"
    else
        die "Test binary not found at '$TEST_BINARY' or '$ALT_BINARY'. Please build first with: cmake --build <build_dir> --target test_database_client"
    fi
fi

if [[ ! -x "$TEST_BINARY" ]]; then
    die "Test binary is not executable: $TEST_BINARY"
fi

# ─── Step 1: Initialise databases ────────────────────────────────────────────
log_info "Initialising databases with: sudo $INIT_SCRIPT"
sudo bash "$INIT_SCRIPT" || die "Database initialisation failed."
log_success "Databases initialised."

# ─── Step 2: Run the test binary ─────────────────────────────────────────────
log_info "Running database client tests: $TEST_BINARY"
echo "────────────────────────────────────────────────────────────────────────"

test_exit_code=0
"$TEST_BINARY" "$@" || test_exit_code=$?

echo "────────────────────────────────────────────────────────────────────────"

if [[ $test_exit_code -eq 0 ]]; then
    log_success "All database client tests passed."
else
    log_error "Some tests failed (exit code: $test_exit_code)."
fi

# cleanup runs automatically via the EXIT trap (stops QuestDB)

exit $test_exit_code
