#!/usr/bin/env -S bash -euET -o pipefail -O inherit_errexit

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
export ARTS_ROOT="${ARTS_ROOT:=}"
export ARTS_NORM="$(var=${ARTS_ROOT:-1}; echo "${var#"${ARTS_ROOT}"}")"
export ARTS_DEBUG="${ARTS_DEBUG:=}"
export ARTS_NDEBUG="$(var=${ARTS_DEBUG:-1}; echo "${var#"${ARTS_DEBUG}"}")"
export ARTS_BIN="${ARTS_BIN:?ARTS_BIN is unset or null}"
export ARTS_MOUNT="${ARTS_MOUNT:?ARTS_MOUNT is unset or null}"
export ARTS_CONFIG="$ARTS_MOUNT/arts/config.yml"
export ARTS_OFFSET="${ARTS_OFFSET:?ARTS_OFFSET is unset or null}"
export ARTS_TEMP="${ARTS_TEMP:?ARTS_TEMP is unset or null}"
export ARTS_FILE="${ARTS_FILE:?ARTS_FILE is unset or null}"
export ARTS_COMPRESSION_LEVEL="${ARTS_COMPRESSION_LEVEL:-3}"
export ARTS_COMPRESSION_SLACK="${ARTS_COMPRESSION_SLACK:-50000}" # 50MB
export ARTS_COMPRESSION_DIRS="${ARTS_COMPRESSION_DIRS:-/usr /opt}"

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
  "$ARTS_BIN"/fuse2fs -o "${ALLOW_OTHER:+allow_other,}"fakeroot,offset="$ARTS_OFFSET" "$ARTS_FILE" "$ARTS_MOUNT"
}

# Unmount the main filesystem
function _unmount()
{
  fusermount -u "$ARTS_MOUNT"
}

# Quits the program
# $* = Termination message
function _die()
{
  [ -z "$*" ] || ARTS_DEBUG=1 _msg "$*"
  # shellcheck disable=2038
  find "$ARTS_MOUNT" -maxdepth 1 -iname "*.dwarfs" -exec basename -s .dwarfs "{}" \; |
    xargs -I{} fusermount -u "$ARTS_TEMP/dwarfs"/{} &>/dev/null || true
  kill -s SIGTERM "$PID"
}

trap _die SIGINT EXIT

function _help()
{
  sed -E 's/^\s+://' <<-EOF
  :Application Chroot Subsystem (Arts), $ARTS_DIST
  :Avaliable options:
  :- arts-compress: Compress the filesystem to a read-only format.
  :- arts-tarball: Install a tarball in '/'.
  :- arts-exec: Execute an artitrary command.
  :- arts-cmd: Set the default command to execute.
  :- arts-resize: Resize the filesystem.
  :- arts-help: Print this message.
	EOF
}

# Changes the filesystem size
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
  cat /tmp/arts/fuse2fs >>  "$ARTS_FILE"
  cat /tmp/arts/dwarfs >>   "$ARTS_FILE"
  cat /tmp/arts/mkdwarfs >> "$ARTS_FILE"

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

  # Mount dwarfs files is exist
  # shellcheck disable=2044
  for i in $(find "$ARTS_MOUNT" -maxdepth 1 -iname "*.dwarfs"); do
    i="$(basename "$i")"
    local fs="$ARTS_MOUNT/$i"
    local mp="$ARTS_TEMP/dwarfs/${i%.dwarfs}"; mkdir -p "$mp"
    local lnk="$ARTS_MOUNT/${i%.dwarfs}"
    rm -f "$lnk" && ln -sf "$mp" "$lnk"
    "$ARTS_BIN/dwarfs" "$fs" "$mp" &>/dev/null
  done

  # Export variables to chroot
  export TERM="xterm"
  export XDG_RUNTIME_DIR="/run/user/$(id -u)"

  # Remove override to avoid problems with apt
  rm ${ARTS_DEBUG:+-v} -f "$ARTS_MOUNT/var/lib/dpkg/statoverride"

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
  # Remove apt lists and cache
  rm -rf "$ARTS_MOUNT"/var/{lib/apt/lists,cache}

  # Create temporary directory to fit-resize fs
  local dir_compressed="$ARTS_TEMP/dir_compressed"; mkdir "$dir_compressed"

  # Compress selected directories
  for i in ${ARTS_COMPRESSION_DIRS}; do
    local target="$ARTS_MOUNT/$i"
    [ -d "$target" ] ||  _die "Folder $target not found for compression"
    "$ARTS_BIN/mkdwarfs" -i "$target" -o "${dir_compressed}/$i.dwarfs" -l"$ARTS_COMPRESSION_LEVEL" -f
    rm -rf "$target"
  done

  # Remove remaining files from dev
  rm -rf "${ARTS_MOUNT:?"Empty ARTS_MOUNT"}"/dev

  # Move files to temporary directory
  mv "$ARTS_MOUNT"/* "$dir_compressed"

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

function _default_cmd_fetch()
{
  if [[ "$(cat "$ARTS_CONFIG")" =~ cmd_default\ \=\ (.*) ]]; then
    echo "${BASH_REMATCH[1]}"
  else
    echo "/bin/bash"
  fi
}

function _default_cmd_set()
{
  local cmd_default="cmd_default = $*"

  if grep "cmd_default" "$ARTS_CONFIG" &>/dev/null; then
    sed -i "s|cmd_default =.*|$cmd_default|" "$ARTS_CONFIG"
  else
    echo "$cmd_default" >> "$ARTS_CONFIG"
  fi
}


function main()
{
  _msg "ARTS_ROOT        : $ARTS_ROOT"
  _msg "ARTS_BIN         : $ARTS_BIN"
  _msg "ARTS_OFFSET      : $ARTS_OFFSET"
  _msg "ARTS_MOUNT       : $ARTS_MOUNT"
  _msg "ARTS_TEMP        : $ARTS_TEMP"
  _msg "ARTS_FILE        : $ARTS_FILE"
  _msg '$*               : '"$*"

  # Check for pacman dirs
  if [ "$ARTS_DIST" = "ARCH" ] && [ ! -d /var/lib/pacman ]; then
    _die "Missing pacman dir, create with 'sudo mkdir -p /var/lib/pacman'"
  fi

  # Check if config is written
  [ -f "$ARTS_CONFIG" ] || touch "$ARTS_CONFIG"

  if [[ "${1:-}" =~ arts-(.*) ]]; then
    case "${BASH_REMATCH[1]}" in
      "compress") _compress ;;
      "tarball") _install_tarball "$2" ;;
      "root") ARTS_ROOT=1; ARTS_NORM="" ;&
      "exec") _exec "${@:2:1}" "${@:3}" ;;
      "cmd") _default_cmd_set "${@:2}" ;;
      "help") _help;;
      *) _help; _die "Unknown arts command" ;;
    esac
  else
    _exec "$(_default_cmd_fetch)" "$*"
  fi

}

main "$@"

#  vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :
