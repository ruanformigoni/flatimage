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

# Perms
export ARTS_ROOT="${ARTS_ROOT:+1}"
export ARTS_NORM="1"
export ARTS_NORM="${ARTS_NORM#"${ARTS_ROOT}"}"

# Mode
export ARTS_RW="${ARTS_RW:+1}"
export ARTS_RO="1"
export ARTS_RO="${ARTS_RO#"${ARTS_RW}"}"

# Debug
export ARTS_DEBUG="${ARTS_DEBUG:+1}"
export ARTS_NDEBUG="1"
export ARTS_NDEBUG="${ARTS_NDEBUG#"${ARTS_DEBUG}"}"

# Paths
export ARTS_BIN="${ARTS_BIN:?ARTS_BIN is unset or null}"
export ARTS_MOUNT="${ARTS_MOUNT:?ARTS_MOUNT is unset or null}"
export ARTS_CONFIG="$ARTS_MOUNT/arts/arts.cfg"
export ARTS_OFFSET="${ARTS_OFFSET:?ARTS_OFFSET is unset or null}"
export ARTS_TEMP="${ARTS_TEMP:?ARTS_TEMP is unset or null}"
export ARTS_FILE="${ARTS_FILE:?ARTS_FILE is unset or null}"

# Compression
export ARTS_COMPRESSION_LEVEL="${ARTS_COMPRESSION_LEVEL:-6}"
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

  fusermount -u "$ARTS_MOUNT"

  # Wait for process to finish
  while kill -0 "$ppid" 2>/dev/null; do
    _msg "Pid $ppid running"
    sleep .5
  done
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
    for i in /tmp/arts/root/"$sha"/mounts/*; do
      # Check if is mounted
      if mount | grep "$i" &>/dev/null; then
        fusermount -u /tmp/arts/root/"$sha"/mounts/"$(basename "$i")" &> "$ARTS_STREAM" || true
      fi
    done
  fi
  # Wait to unmount
  sleep .5
  # Unmount image
  _unmount &> "$ARTS_STREAM"
  # Exit
  kill -s SIGTERM "$PID"
}

trap _die SIGINT EXIT

function _help()
{
  sed -E 's/^\s+://' <<-EOF
  :Application Root Subsystem (Arts), $ARTS_DIST
  :Avaliable options:
  :- arts-compress: Compress the filesystem to a read-only format.
  :- arts-tarball: Install a tarball in the container's '/'.
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
  e2fsck -fy "$ARTS_FILE"\?offset="$ARTS_OFFSET" || true
  resize2fs "$ARTS_FILE"\?offset="$ARTS_OFFSET" "$1"
  e2fsck -fy "$ARTS_FILE"\?offset="$ARTS_OFFSET" || true

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
  cp "$ARTS_TEMP/runner" "$ARTS_FILE"

  # Append static requirements
  # shellcheck disable=2129
  cat /tmp/arts/proot >>    "$ARTS_FILE"
  cat /tmp/arts/ext2rd >>    "$ARTS_FILE"
  cat /tmp/arts/fuse2fs >>  "$ARTS_FILE"
  cat /tmp/arts/dwarfs >>   "$ARTS_FILE"
  cat /tmp/arts/mkdwarfs >> "$ARTS_FILE"

  # Update offset
  ARTS_OFFSET="$(du -sb "$ARTS_FILE" | awk '{print $1}')"

  # Create filesystem
  truncate -s "$1" "$ARTS_TEMP/image.arts"
  mke2fs -d "$2" -b1024 -t ext2 "$ARTS_TEMP/image.arts"

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
  local cmd="${*:?"Empty arguments for exec"}"

  # Fetch SHA
  local sha="$(_config_fetch "sha")"
  _msg "sha: $sha"

  # Mount dwarfs files is exist
  # shellcheck disable=2044
  for i in $(find "$ARTS_MOUNT" -maxdepth 1 -iname "*.dwarfs"); do
    i="$(basename "$i")"
    local fs="$ARTS_MOUNT/$i"
    local mp="/tmp/arts/root/$sha/mounts/${i%.dwarfs}"; mkdir -p "$mp"
    "$ARTS_BIN/dwarfs" "$fs" "$mp" &> "$ARTS_STREAM"
  done

  # Export variables to chroot
  export TERM="xterm"
  export XDG_RUNTIME_DIR="/run/user/$(id -u)"
  export HOST_USERNAME="$(whoami)"

  # Remove override to avoid problems with apt
  [ -n "$ARTS_RO" ] || rm ${ARTS_DEBUG:+-v} -f "$ARTS_MOUNT/var/lib/dpkg/statoverride"

  # Run proot
  declare -a _cmd_proot

  _cmd_proot+=("$ARTS_BIN/proot")
  _cmd_proot+=("${ARTS_NDEBUG:+--verbose=-1}")
  _cmd_proot+=("${ARTS_ROOT:+-S \"$ARTS_MOUNT\"}")
  _cmd_proot+=("${ARTS_NORM:+-R \"$ARTS_MOUNT\"}")
  _cmd_proot+=("/bin/bash -c \"$cmd\"")

  _msg "cmd_proot: ${_cmd_proot[*]}"

  eval "${_cmd_proot[*]}"
}

# Subdirectory compression
function _compress()
{
  [ -n "$ARTS_RW" ] || _die "Set ARTS_RW to 1 before compression"
  [ -z "$(_config_fetch "sha")" ] || _die "sha is set (already compressed?)"

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
    ln -sf "/tmp/arts/root/$sha/mounts/$i" "${dir_compressed}/${i}"
  done


  # Remove remaining files from dev
  rm -rf "${ARTS_MOUNT:?"Empty ARTS_MOUNT"}"/dev

  # Move files to temporary directory
  for i in "$ARTS_MOUNT"/{arts,bin,etc,lib,lib64,opt,root,run,sbin,share,tmp,usr,var}; do
    [ ! -d "$i" ] || mv "$i" "$dir_compressed"
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
}


# Deploy a tarball in arts
# $1 tarball
# $2 main binary to execute
function _install_tarball()
{
  local tarball="$1"

  # Validate files
  [ -f "$tarball" ] || _die "Invalid tarball $tarball"

  _msg "Tarball: $tarball"

  # Get depth to binaries
  local depth
  read -r depth < <( tar tf "$tarball" |
    grep -Ei '.*/$' |
    pcregrep -o1 "(.*bin/|.*share/|.*lib/)" |
    head -n1 |
    awk -F/ '{print NF-1}' )
  depth="$((depth-1))"

  _msg "Depth: $depth"

  # Copy tarball files
  _msg "Copying files to $ARTS_MOUNT..."
  dir_temp="$(mktemp -d)"
  tar -xf "$tarball" --strip-components="$depth" -C"$dir_temp" 
  rsync -K -a "$dir_temp/" "$ARTS_MOUNT"
  rm -rf "$dir_temp"
}

function _config_fetch()
{
  local opt="$1"

  [ -f "$ARTS_CONFIG" ] || { echo ""; exit; }

  pcregrep -o1 "$opt = (.*)" "$ARTS_CONFIG"
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
  _msg "ARTS_BIN         : $ARTS_BIN"
  _msg "ARTS_OFFSET      : $ARTS_OFFSET"
  _msg "ARTS_MOUNT       : $ARTS_MOUNT"
  _msg "ARTS_TEMP        : $ARTS_TEMP"
  _msg "ARTS_FILE        : $ARTS_FILE"
  _msg '$*               : '"$*"

  # Check filesystem
  e2fsck -fy "$ARTS_FILE"\?offset="$ARTS_OFFSET" &> "$ARTS_STREAM" || true

  # Mount filesystem
  _mount

  # Check for pacman dirs
  if [ "$ARTS_DIST" = "ARCH" ] && [ ! -d /var/lib/pacman ]; then
    _die "Missing pacman dir, create with 'sudo mkdir -p /var/lib/pacman'"
  fi

  # Check if config exists, else try to touch if mounted as RW
  [ -f "$ARTS_CONFIG" ] || { [ -n "$ARTS_RO" ] || touch "$ARTS_CONFIG"; }

  if [[ "${1:-}" =~ arts-(.*) ]]; then
    case "${BASH_REMATCH[1]}" in
      "compress") _compress ;;
      "tarball") _install_tarball "$2" ;;
      "root") ARTS_ROOT=1; ARTS_NORM="" ;&
      "exec") _exec "${@:2:1}" "${@:3}" ;;
      "cmd") _config_set "cmd" "${@:2}" ;;
      "resize") _resize "$2" ;;
      "xdg") _re_mount "$2"; xdg-open "$2"; read -r ;;
      "mount") _re_mount "$2"; read -r ;;
      "help") _help;;
      *) _help; _die "Unknown arts command" ;;
    esac
  else
    local cmd="$(_config_fetch "cmd")"
    _exec  "${cmd:-/bin/bash}" "$*"
  fi

}

main "$@"

#  vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :
