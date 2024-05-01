######################################################################
# @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
# @file        : _daemon_killer
# @created     : Saturday Dec 23, 2023 22:11:06 -03
#
# @description : Handles flatimage mounts after finished execution
######################################################################

exec 1> >(while IFS= read -r line; do echo "$line" | tee -a "${FIM_DIR_MOUNT}.killer.log"; done)
exec 2> >(while IFS= read -r line; do echo "$line" | tee -a "${FIM_DIR_MOUNT}.killer.log" >&2; done)

shopt -s nullglob

# _msg() {{{
# Writes a message to logfile
# $(1..n-1) arguments to echo
# $n message
function _msg()
{
  echo -e "${@:1:${#@}-1}" "[*] ${*: -1}" >&2;
}
# }}}

# _wait_kill() {{{
# Wait for a pid to finish execution, similar to 'wait'
# but also works for non-child pids
# Kills on timeout
# $1: message
# $2: pid
# $3: timeout
function _wait_kill()
{
  # Get pid
  declare msg="$1"
  declare -i pid="$2"
  declare -i limit="${3:-50}"

  # Ignore pid 0, this might happen if pgrep
  # was done after process exit
  if test "$pid" -eq 0 || test -z "$pid"; then
    _msg "Pid $pid ignore..."
  fi

  # Print message
  _msg "[ $pid ] : $msg"

  # Wait for process to finish
  # ...or kill on timeout
  declare ms_sleep=0.1
  declare -i iterations=0
  while kill -0 "$pid" 2>/dev/null; do
    _msg "Pid $pid running with sleep ${ms_sleep}ms, $iterations of $limit iterations..."
    iterations+=1
    sleep "$ms_sleep"
    if test "$iterations" -gt "$limit"; then
      kill -s SIGKILL "$pid"
      _msg "Pid $pid killed..."
      break
    fi
  done
  _msg "Pid $pid finished..."
}
# }}}

# _die() {{{
# Quits the program
# $* = Termination message
function _die()
{
  # Check if should continue
  if [ -f "${FIM_DIR_MOUNT}.killer.stop" ]; then
    _msg "Disarmed"
    return;
  fi

  # Force debug message
  [ -z "$*" ] || FIM_DEBUG=1 _msg "$*"

  # Close portal
  pkill --signal SIGTERM -f "portal_host $FIM_FILE_BINARY"

  # Unmount overlayfs
  for i in "${FIM_DIR_MOUNTS_OVERLAYFS}"/*; do
    # Iterate on parent and child pids
    _msg "Unmount $i"
    for pid in $(pgrep -f "$i"); do
      # Ignore self
      if [ "$pid" = "$$" ]; then continue; fi
      # Send unmount signal to dwarfs mountpoint
      fusermount -u "$i"
      # Wait and kill if it takes too long
      _wait_kill "Wait for unmount of overlayfs in $i" "$pid"
    done
    # Send again, now with the process released
    fusermount -u "$i"
  done

  # Unmount dwarfs
  for i in "${FIM_DIR_MOUNTS_DWARFS}"/*; do
    _msg "Unmount $i"
    for pid in $(pgrep -f "$i"); do
      # Ignore self
      if [ "$pid" = "$$" ]; then continue; fi
      # Send unmount signal to dwarfs mountpoint
      fusermount -u "$i"
      # Wait and kill if it takes too long
      _wait_kill "Wait for unmount of dwarfs in $i" "$pid"
    done
    # Send again, now with the process released
    fusermount -u "$i"
  done

  # Unmount fuse2fs
  fusermount -u "$FIM_DIR_MOUNT"

  # Wait for processes using $FIM_FILE_BINARY or kill on timeout
  while read -r i; do
    # Ignore self
    if [ "$i" = "$$" ]; then continue; fi
    # Process PID
    _msg "Process using '$FIM_FILE_BINARY' is '$(file "/proc/$i/exe")'"
    _wait_kill "Wait for process '$i' to stop using '$FIM_FILE_BINARY', timeout 6s" "$i" 60
  done < <(lsof -t "$FIM_FILE_BINARY")

  # Kill the process if unmount does not work after a timeout
  for pid in $(pgrep -f "fuse2fs.*offset=$FIM_OFFSET.*$FIM_FILE_BINARY.*$FIM_DIR_MOUNT"); do
    # Ignore self
    if [ "$pid" = "$$" ]; then continue; fi
    # Wait and kill if it takes too long
    _wait_kill "Wait for unmount of fuse2fs in $FIM_DIR_MOUNT" "$pid"
  done

  # Try again
  # # Now process of fuse2fs is dead
  # # Now no one is using FIM_DIR_MOUNT
  fusermount -u "$FIM_DIR_MOUNT"

  # Force exit if killer is daemon
  if [[ "$$" != "$PID" ]]; then
    _msg "Killing pid '$PID', self is '$$'"
    kill -s SIGKILL "$PID"
  else
    # Success, disarm daemon
    touch "${FIM_DIR_MOUNT}.killer.stop"
  fi

}
trap _die SIGINT EXIT
# }}}

# _main() {{{
function main()
{
  [ -v PID             ] || _msg "PID is not defined"
  [ -v FIM_DIR_MOUNT   ] || _msg "FIM_DIR_MOUNT is not defined"
  [ -v FIM_FILE_BINARY ] || _msg "FIM_FILE_BINARY is not defined"

  # Used when main script becomes the killer
  if [[ "$1" == "nowait" ]]; then
    return;
  fi

  # Used on daemon to wait for the boot process to finish
  while kill -0 "$PID" 2>/dev/null; do
    if [ -f "${FIM_DIR_MOUNT}.killer.kill" ]; then
      _msg "Manually killed"
      break
    fi
    sleep 1
  done
}
# }}}

main "$@"

#  vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :
