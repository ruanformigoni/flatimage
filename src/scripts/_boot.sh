#!/usr/bin/env bash

######################################################################
# @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
# @file        : _boot
# @created     : Monday Jan 23, 2023 21:18:26 -03
#
# @description : Boot arts in chroot
######################################################################

#shellcheck disable=2155

set -e

PID="$$"

export ARTS_DIST="TRUNK"

# Rootless tool
export ARTS_TOOL="${ARTS_TOOL:-bwrap}"

# Perms
export ARTS_ROOT="${ARTS_ROOT:+1}"
export ARTS_NORM="1"
export ARTS_NORM="${ARTS_NORM#"${ARTS_ROOT}"}"

# Mode
export ARTS_RO="${ARTS_RO:+1}"
export ARTS_RW="1"
export ARTS_RW="${ARTS_RW#"${ARTS_RO}"}"

# Debug
export ARTS_DEBUG="${ARTS_DEBUG:+1}"
export ARTS_NDEBUG="1"
export ARTS_NDEBUG="${ARTS_NDEBUG#"${ARTS_DEBUG}"}"

# Paths
export ARTS_BASE="${ARTS_BASE:?ARTS_BASE is unset or null}"
export ARTS_BIN="${ARTS_BASE}/bin"
export ARTS_MOUNT="${ARTS_MOUNT:?ARTS_MOUNT is unset or null}"
export ARTS_CONFIG="$ARTS_MOUNT/arts/arts.cfg"
export ARTS_OFFSET="${ARTS_OFFSET:?ARTS_OFFSET is unset or null}"
export ARTS_SECTOR=$((ARTS_OFFSET/512))
export ARTS_TEMP="${ARTS_TEMP:?ARTS_TEMP is unset or null}"
export ARTS_FILE="${ARTS_FILE:?ARTS_FILE is unset or null}"
export ARTS_RCFILE="$ARTS_TEMP/.bashrc"

# Compression
export ARTS_COMPRESSION_LEVEL="${ARTS_COMPRESSION_LEVEL:-4}"
export ARTS_COMPRESSION_SLACK="${ARTS_COMPRESSION_SLACK:-50000}" # 50MB
export ARTS_COMPRESSION_DIRS="${ARTS_COMPRESSION_DIRS:-/usr /opt}"

# Output stream
export ARTS_STREAM="${ARTS_DEBUG:+/dev/stdout}"
export ARTS_STREAM="${ARTS_STREAM:-/dev/null}"

# Emits a message in &2
# $(1..n-1) arguments to echo
# $n message
function _msg()
{
  [ -z "$ARTS_DEBUG" ] || echo -e "${@:1:${#@}-1}" "[\033[32m*\033[m] ${*: -1}" >&2;
}

# Wait for a pid to finish execution, similar to 'wait'
# but also works for non-child pids
# $1: pid
function _wait()
{
  # Get pid
  local pid="$1"

  # Wait for process to finish
  while kill -0 "$pid" 2>/dev/null; do
    _msg "Pid $pid running..."
    sleep .1
  done
  _msg "Pid $pid finished..."
}

# Mount the main filesystem
function _mount()
{
  local mode="${ARTS_RW:-ro,}"
  local mode="${mode#1}"
  "$ARTS_BIN"/fuse2fs -o "$mode"fakeroot,offset="$ARTS_OFFSET" "$ARTS_FILE" "$ARTS_MOUNT" &> "$ARTS_STREAM"
}

# Unmount the main filesystem
function _unmount()
{
  # Get parent pid
  local ppid="$(pgrep -f "fuse2fs.*offset=$ARTS_OFFSET.*$ARTS_FILE")"

  fusermount -zu "$ARTS_MOUNT"

  _wait "$ppid"
}

# Re-mount the filesystem in new mountpoint
# $1 New mountpoint
function _re_mount()
{
  # Umount from initial mountpoint
  _unmount
  # Mount in new mountpoint
  export ARTS_MOUNT="$1"; _mount
}

# Quits the program
# $* = Termination message
function _die()
{
  [ -z "$*" ] || ARTS_DEBUG=1 _msg "$*"
  # Unmount dwarfs
  local sha="$(_config_fetch "sha")"
  if [ -n "$sha" ]; then
    shopt -s nullglob
    for i in "$ARTS_BASE"/root/"$sha"/mounts/*; do
      # Check if is mounted
      if mount | grep "$i" &>/dev/null; then
        # Get parent pid
        local ppid="$(pgrep -f "dwarfs2.*$i")"
        fusermount -zu "$ARTS_BASE"/root/"$sha"/mounts/"$(basename "$i")" &> "$ARTS_STREAM" || true
        _wait "$ppid"
      fi
    done
  fi
  # Unmount image
  _unmount &> "$ARTS_STREAM"
  # Exit
  kill -s SIGTERM "$PID"
}

trap _die SIGINT EXIT

function _copy_tools()
{
  ARTS_RO=1 ARTS_RW="" _mount

  for i; do
    local tool="$i"

    if [ ! -f "$ARTS_BIN"/"$tool" ]; then
      cp "$ARTS_MOUNT/arts/static/$tool" "$ARTS_BIN"
      chmod +x "$ARTS_BIN"/"$tool"
    fi
  done

  _unmount
}

function _help()
{
  sed -E 's/^\s+://' <<-EOF
  :Application Root Subsystem (Arts), $ARTS_DIST
  :Avaliable options:
  :- arts-compress: Compress the filesystem to a read-only format.
  :- arts-exec: Execute an arbitrary command.
  :- arts-root: Execute an arbitrary command as root.
  :- arts-cmd: Set the default command to execute.
  :- arts-resize: Resize the filesystem.
  :- arts-mount: Mount the filesystem in a specified directory
  :    - E.g.: ./focal.arts arts-mount ./mountpoint
  :- arts-xdg: Same as the 'arts-mount' command, however it opens the
  :    mount directory with xdg-open
  :- arts-help: Print this message.
	EOF
}

# Changes the filesystem size
# $1 New sise
function _resize()
{
  # Unmount
  _unmount

  # Resize
  "$ARTS_BIN"/e2fsck -fy "$ARTS_FILE"\?offset="$ARTS_OFFSET" || true
  "$ARTS_BIN"/resize2fs "$ARTS_FILE"\?offset="$ARTS_OFFSET" "$1"
  "$ARTS_BIN"/e2fsck -fy "$ARTS_FILE"\?offset="$ARTS_OFFSET" || true

  # Mount
  _mount
}

# Re-create the filesystem with new data
# $1 New size
# $2 Dir to create image from
function _rebuild()
{
  _unmount

  # Erase current file
  rm "$ARTS_FILE"

  # Copy startup binary
  cp "$ARTS_BIN/main" "$ARTS_FILE"

  # Append tools
  cat "$ARTS_BIN"/{ext2rd,fuse2fs,e2fsck}  >> "$ARTS_FILE"

  # Update offset
  ARTS_OFFSET="$(du -sb "$ARTS_FILE" | awk '{print $1}')"

  # Create filesystem
  truncate -s "$1" "$ARTS_TEMP/image.arts"
  "$ARTS_BIN"/mke2fs -d "$2" -b1024 -t ext2 "$ARTS_TEMP/image.arts"

  # Append filesystem to binary
  cat "$ARTS_TEMP/image.arts" >> "$ARTS_FILE"

  # Remove filesystem
  rm "$ARTS_TEMP/image.arts"

  # Re-mount
  _mount
}

# Chroots into the filesystem
# $* Command and args
function _exec()
{
  # Check for empty string
  [ -n "$*" ] || ARTS_DEBUG=1 _msg "Empty arguments for exec"

  # Fetch CMD
  local cmd="$*"
  _msg "cmd: $cmd"

  # Fetch SHA
  local sha="$(_config_fetch "sha")"
  _msg "sha: $sha"

  # Mount dwarfs files if exist
  [ -f "$ARTS_BIN/dwarfs" ]  || cp "$ARTS_MOUNT/arts/static/dwarfs" "$ARTS_BIN"/dwarfs
  chmod +x "$ARTS_BIN/dwarfs"

  # shellcheck disable=2044
  for i in $(find "$ARTS_MOUNT" -maxdepth 1 -iname "*.dwarfs"); do
    i="$(basename "$i")"
    local fs="$ARTS_MOUNT/$i"
    local mp="$ARTS_BASE/root/$sha/mounts/${i%.dwarfs}"; mkdir -p "$mp"
    "$ARTS_BIN/dwarfs" "$fs" "$mp" &> "$ARTS_STREAM"
  done

  # Export variables to chroot
  export TERM="xterm"
  export XDG_RUNTIME_DIR="/run/user/$(id -u)"
  export HOST_USERNAME="$(whoami)"
  export PATH="$PATH:/sbin:/usr/sbin:/usr/local/sbin:/bin:/usr/bin:/usr/local/bin"
  tee "$ARTS_RCFILE" &>/dev/null <<- 'EOF'
    export PS1="(arts@$(echo "$ARTS_DIST" | tr '[:upper:]' '[:lower:]')) → "
	EOF

  # Remove override to avoid problems with apt
  [ -n "$ARTS_RO" ] || rm ${ARTS_DEBUG:+-v} -f "$ARTS_MOUNT/var/lib/dpkg/statoverride"

  declare -a _cmd

  # Fetch permissions
  source "$ARTS_MOUNT/arts/perms"

  # Run in container
  if [[ "$ARTS_TOOL" = "bwrap" ]]; then
    _msg "Using bubblewrap"

    # Main binary
    _cmd+=("$ARTS_BIN/bwrap")

    # Root binding
    _cmd+=("${ARTS_ROOT:+--uid 0 --gid 0}")

    # Path to subsystem
    _cmd+=("--bind \"$ARTS_MOUNT\" /")

    # User home
    _cmd+=("--bind \"$HOME\" \"$HOME\"")

    # System bindings
    _cmd+=("--dev /dev")
    _cmd+=("--proc /proc")
    _cmd+=("--bind /tmp /tmp")
    _cmd+=("--bind /sys /sys")

    # Pulseaudio
    if [[ "$ARTS_PERM_PULSEAUDIO" -eq 1 ]]; then
      _msg "PERM: Pulseaudio"
      local PULSE_SOCKET="$XDG_RUNTIME_DIR/pulse/native"
      _cmd+=("--setenv PULSE_SERVER unix:$PULSE_SOCKET")
      _cmd+=("--bind $PULSE_SOCKET $PULSE_SOCKET")
    fi

    # Wayland
    if [[ "$ARTS_PERM_WAYLAND" -eq 1 ]]; then
      _msg "PERM: Wayland"
      local WAYLAND_SOCKET_PATH="$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY"
      _cmd+=("--bind $WAYLAND_SOCKET_PATH $WAYLAND_SOCKET_PATH")
      _cmd+=("--setenv WAYLAND_DISPLAY $WAYLAND_DISPLAY")
      _cmd+=("--setenv XDG_RUNTIME_DIR $XDG_RUNTIME_DIR")
    fi

    # X11
    if [[ "$ARTS_PERM_X11" -eq 1 ]]; then
      _msg "PERM: X11"
      _cmd+=("--setenv DISPLAY $DISPLAY")
      _cmd+=("--setenv XAUTHORITY $XAUTHORITY")
      _cmd+=("--ro-bind $XAUTHORITY $XAUTHORITY")
    fi

    # dbus (user)
    if [[ "$ARTS_PERM_SESSION_BUS" -eq 1 ]]; then
      _msg "PERM: SESSION BUS"
      _cmd+=("--setenv DBUS_SESSION_BUS_ADDRESS $DBUS_SESSION_BUS_ADDRESS")
      _cmd+=("--bind ${DBUS_SESSION_BUS_ADDRESS#*=} ${DBUS_SESSION_BUS_ADDRESS#*=}")
    fi

    # dbus (system)
    if [[ "$ARTS_PERM_SYSTEM_BUS" -eq 1 ]]; then
      _msg "PERM: SYSTEM BUS"
      _cmd+=("--setenv DBUS_SESSION_BUS_ADDRESS $DBUS_SESSION_BUS_ADDRESS")
      _cmd+=("--bind /run/dbus/system_bus_socket /run/dbus/system_bus_socket")
    fi

    # GPU
    if [[ "$ARTS_PERM_GPU" -eq 1 ]]; then
      _msg "PERM: GPU"
      _cmd+=("--dev-bind /dev/dri /dev/dri")
    fi

    # Host info
    [ ! -f "/etc/host.conf"     ] || _cmd+=('--bind "/etc/host.conf"     "/etc/host.conf"')
    [ ! -f "/etc/hosts"         ] || _cmd+=('--bind "/etc/hosts"         "/etc/hosts"')
    [ ! -f "/etc/passwd"        ] || _cmd+=('--bind "/etc/passwd"        "/etc/passwd"')
    [ ! -f "/etc/group"         ] || _cmd+=('--bind "/etc/group"         "/etc/group"')
    [ ! -f "/etc/nsswitch.conf" ] || _cmd+=('--bind "/etc/nsswitch.conf" "/etc/nsswitch.conf"')
    [ ! -f "/etc/resolv.conf"   ] || _cmd+=('--bind "/etc/resolv.conf"   "/etc/resolv.conf"')
  else
    _msg "Using proot"

    # Main binary
    _cmd+=("$ARTS_BIN/proot")

    # Root binding
    _cmd+=("-0")

    # Path to subsystem
    _cmd+=("-r \"$ARTS_MOUNT\"")

    # User home
    _cmd+=("-b \"$HOME\"")

    # System bindings
    _cmd+=("-b /dev")
    _cmd+=("-b /proc")
    _cmd+=("-b /tmp")
    _cmd+=("-b /sys")

    # Pulseaudio
    if [[ "$ARTS_PERM_PULSEAUDIO" -eq 1 ]]; then
      _msg "PERM: Pulseaudio"
      local PULSE_SOCKET="$XDG_RUNTIME_DIR/pulse/native"
      export PULSE_SERVER="unix:$PULSE_SOCKET"
      _cmd+=("-b $PULSE_SOCKET")
    fi

    # Wayland
    if [[ "$ARTS_PERM_WAYLAND" -eq 1 ]]; then
      _msg "PERM: Wayland"
      local WAYLAND_SOCKET_PATH="$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY"
      _cmd+=("-b $WAYLAND_SOCKET_PATH")
      export WAYLAND_DISPLAY="$WAYLAND_DISPLAY"
      export XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR"
    fi

    # X11
    if [[ "$ARTS_PERM_X11" -eq 1 ]]; then
      _msg "PERM: X11"
      export DISPLAY="$DISPLAY"
      export XAUTHORITY="$XAUTHORITY"
      _cmd+=("-b $XAUTHORITY")
    fi

    # dbus (user)
    if [[ "$ARTS_PERM_SESSION_BUS" -eq 1 ]]; then
      _msg "PERM: SESSION BUS"
      export DBUS_SESSION_BUS_ADDRESS="$DBUS_SESSION_BUS_ADDRESS"
      _cmd+=("-b ${DBUS_SESSION_BUS_ADDRESS#*=}")
    fi

    # dbus (system)
    if [[ "$ARTS_PERM_SYSTEM_BUS" -eq 1 ]]; then
      _msg "PERM: SYSTEM BUS"
      _cmd+=("-b /run/dbus/system_bus_socket")
    fi

    # GPU
    if [[ "$ARTS_PERM_GPU" -eq 1 ]]; then
      _msg "PERM: GPU"
      _cmd+=("-b /dev/dri")
    fi

    # Host info
    [ ! -f "/etc/host.conf"     ] || _cmd+=('-b "/etc/host.conf"')
    [ ! -f "/etc/hosts"         ] || _cmd+=('-b "/etc/hosts"')
    [ ! -f "/etc/passwd"        ] || _cmd+=('-b "/etc/passwd"')
    [ ! -f "/etc/group"         ] || _cmd+=('-b "/etc/group"')
    [ ! -f "/etc/nsswitch.conf" ] || _cmd+=('-b "/etc/nsswitch.conf"')
    [ ! -f "/etc/resolv.conf"   ] || _cmd+=('-b "/etc/resolv.conf"')
  fi

  # Shell
  _cmd+=("/bin/bash -c '$cmd'")

  eval "${_cmd[*]}"
}

# Subdirectory compression
function _compress()
{
  [ -n "$ARTS_RW" ] || _die "Set ARTS_RW to 1 before compression"
  [ -z "$(_config_fetch "sha")" ] || _die "sha is set (already compressed?)"

  # Copy compressor to binary dir
  [ -f "$ARTS_BIN/mkdwarfs" ]  || cp "$ARTS_MOUNT/arts/static/mkdwarfs" "$ARTS_BIN"/mkdwarfs
  chmod +x "$ARTS_BIN/mkdwarfs"

  # Remove apt lists and cache
  rm -rf "$ARTS_MOUNT"/var/{lib/apt/lists,cache}

  # Create temporary directory to fit-resize fs
  local dir_compressed="$ARTS_TEMP/dir_compressed"; mkdir "$dir_compressed"

  # Get SHA and save to re-mount (used as unique identifier)
  local sha="$(sha256sum "$ARTS_FILE" | awk '{print $1}')"
  _config_set "sha" "$sha"
  _msg "sha: $sha"

  # Compress selected directories
  for i in ${ARTS_COMPRESSION_DIRS}; do
    local target="$ARTS_MOUNT/$i"
    [ -d "$target" ] ||  _die "Folder $target not found for compression"
    "$ARTS_BIN/mkdwarfs" -i "$target" -o "${dir_compressed}/$i.dwarfs" -l"$ARTS_COMPRESSION_LEVEL" -f
    rm -rf "$target"
    ln -sf "$ARTS_BASE/root/$sha/mounts/$i" "${dir_compressed}/${i}"
  done


  # Remove remaining files from dev
  rm -rf "${ARTS_MOUNT:?"Empty ARTS_MOUNT"}"/dev

  # Move files to temporary directory
  for i in "$ARTS_MOUNT"/{arts,bin,etc,lib,lib64,opt,root,run,sbin,share,tmp,usr,var}; do
    { mv "$i" "$dir_compressed" || true; } &>"$ARTS_STREAM"
  done

  # Update permissions
  chmod -R +rw "$dir_compressed"

  # Resize to fit files size + slack
  local size_files="$(du -s -BK "$dir_compressed" | awk '{ gsub("K","",$1); print $1}')"
  local size_offset="$((ARTS_OFFSET/1024))" # Bytes to K
  local size_slack="$ARTS_COMPRESSION_SLACK";
  size_new="$((size_files+size_offset+size_slack))"

  _msg "Size files  : $size_files"
  _msg "Size offset : $size_files"
  _msg "Size slack  : $size_slack"
  _msg "Size sum    : $size_new"

  # Resize
  _rebuild "$size_new"K "$dir_compressed"

  # Remove mount dirs
  rm -rf "${ARTS_MOUNT:?"Empty mount var"}"/{tmp,proc,sys,dev,run}

  # Create required mount points if not exists
  mkdir -p "$ARTS_MOUNT"/{tmp,proc,sys,dev,run,home}
}

function _config_fetch()
{
  local opt="$1"

  [ -f "$ARTS_CONFIG" ] || { echo ""; exit; }

  grep -io "$opt = .*" "$ARTS_CONFIG" | awk '{print $3}'
}

function _config_set()
{
  local opt="$1"; shift
  local entry="$opt = $*"

  if grep "$opt" "$ARTS_CONFIG" &>"$ARTS_STREAM"; then
    sed -i "s|$opt =.*|$entry|" "$ARTS_CONFIG"
  else
    echo "$entry" >> "$ARTS_CONFIG"
  fi
}

function main()
{
  _msg "ARTS_RO          : $ARTS_RO"
  _msg "ARTS_RW          : $ARTS_RW"
  _msg "ARTS_STREAM      : $ARTS_STREAM"
  _msg "ARTS_ROOT        : $ARTS_ROOT"
  _msg "ARTS_NORM        : $ARTS_NORM"
  _msg "ARTS_DEBUG       : $ARTS_DEBUG"
  _msg "ARTS_NDEBUG      : $ARTS_NDEBUG"
  _msg "ARTS_BASE        : $ARTS_BASE"
  _msg "ARTS_BIN         : $ARTS_BIN"
  _msg "ARTS_OFFSET      : $ARTS_OFFSET"
  _msg "ARTS_MOUNT       : $ARTS_MOUNT"
  _msg "ARTS_TEMP        : $ARTS_TEMP"
  _msg "ARTS_FILE        : $ARTS_FILE"
  _msg '$*               : '"$*"

  # Check filesystem
  "$ARTS_BIN"/e2fsck -fy "$ARTS_FILE"\?offset="$ARTS_OFFSET" &> "$ARTS_STREAM" || true

  # Copy tools
  _copy_tools "proot" "fuse2fs" "e2fsck" "resize2fs" "mke2fs" "bwrap"

  # Mount filesystem
  _mount

  # Check for pacman dirs
  if [ "$ARTS_DIST" = "ARCH" ] && [ ! -d /var/lib/pacman ]; then
    ARTS_DEBUG=1 _msg "Missing pacman dir, create it with 'sudo mkdir -p /var/lib/pacman' if you want to use pacman"
  fi

  # Check if config exists, else try to touch if mounted as RW
  [ -f "$ARTS_CONFIG" ] || { [ -n "$ARTS_RO" ] || touch "$ARTS_CONFIG"; }

  if [[ "${1:-}" =~ arts-(.*) ]]; then
    case "${BASH_REMATCH[1]}" in
      "compress") _compress ;;
      "root") ARTS_ROOT=1; ARTS_NORM="" ;&
      "exec") shift; _exec "$@" ;;
      "cmd") _config_set "cmd" "${@:2}" ;;
      "resize") _resize "$2" ;;
      "xdg") _re_mount "$2"; xdg-open "$2"; read -r ;;
      "mount") _re_mount "$2"; read -r ;;
      "help") _help;;
      *) _help; _die "Unknown arts command" ;;
    esac
  else
    local default_cmd="$(_config_fetch "cmd")"
    _exec  "${default_cmd:-/bin/bash --rcfile "$ARTS_RCFILE"}" "$*"
  fi

}

main "$@"

#  vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :
