set -e

DIR_SCRIPT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

readonly PATH_FILE_BASH="$(command -v bash)"
readonly EACCES=13

# Log file
BWRAP_LOG="${BWRAP_LOG:?variable is undefined}"

# Redirect streams

# Currently defined bwrap binary
PATH_FILE_BWRAP_PATH="$(command -v bwrap)"
# Target to copy bwrap binary to
PATH_FILE_BWRAP_APPARMOR=/opt/bwrap/bwrap


function bwrap_test_and_run_source()
{
  "$PATH_FILE_BWRAP_PATH" --bind / / "$PATH_FILE_BASH" -c echo &>>"$BWRAP_LOG" || return $?
  echo "Using bwrap '$PATH_FILE_BWRAP_PATH'" &>>"$BWRAP_LOG"
  "$PATH_FILE_BWRAP_PATH" "$@"
}

function bwrap_test_and_run_apparmor()
{
  "$PATH_FILE_BWRAP_APPARMOR" --bind / / "$PATH_FILE_BASH" -c echo &>>"$BWRAP_LOG" || return $?
  echo "Using bwrap '$PATH_FILE_BWRAP_APPARMOR'" &>>"$BWRAP_LOG"
  "$PATH_FILE_BWRAP_APPARMOR" "$@"
}

function main()
{
  # Test already configured apparmor bwrap if exists
  if bwrap_test_and_run_apparmor "$@"; then
    exit
  fi

  # Check if bwrap works without issues
  if bwrap_test_and_run_source "$@"; then
    exit
  else
    code_fail=$?
    echo "Bubblewrap test failed with code '$code_fail'" &>>"$BWRAP_LOG"
  fi

  # Try to find pkexec binary
  if ! PATH_BIN_PKEXEC="$(command -v pkexec)"; then
    echo "Could not find pkexec binary"
    exit 1
  fi &>>"$BWRAP_LOG"

  # Try to integrate apparmor profile if error is EACCES
  if [[ "$code_fail" == "$EACCES" ]]; then
    echo "Failed with EACCESS, trying to create an apparmor profile"
    "$PATH_BIN_PKEXEC" "$PATH_FILE_BASH" "$DIR_SCRIPT/bwrap-profile.sh" "$PATH_FILE_BWRAP_PATH" "$PATH_FILE_BWRAP_APPARMOR"
  fi &>>"$BWRAP_LOG"

  # Re-test apparmor binary
  if bwrap_test_and_run_apparmor "$@"; then
    exit
  fi

  # Use pkexec with bwrap as a last resort
  echo "Apparmor configuration failed" &>>"$BWRAP_LOG"
}

main "$@"

# vim: set ts=2 sw=2 nu rnu et:
