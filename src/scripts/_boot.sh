#!/tmp/fim/bin/bash

######################################################################
# @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
# @file        : _boot
# @created     : Monday Jan 23, 2023 21:18:26 -03
#
# @description : Boot fim in chroot
######################################################################

#shellcheck disable=2155

set "${FIM_DEBUG_SET_ARGS:-"-e"}"

export PID="$$"

export FIM_DIST=ARCH

# Rootless tool
export FIM_BACKEND

# Perms
export FIM_ROOT="${FIM_ROOT:+1}"
export FIM_NORM="1"
export FIM_NORM="${FIM_NORM#"${FIM_ROOT}"}"

# Mode
export FIM_RO="${FIM_RO:+1}"
export FIM_RW="1"
export FIM_RW="${FIM_RW#"${FIM_RO}"}"

# Debug
export FIM_DEBUG="${FIM_DEBUG:+1}"
export FIM_NDEBUG="1"
export FIM_NDEBUG="${FIM_NDEBUG#"${FIM_DEBUG}"}"

# Filesystem offset
export FIM_OFFSET="${FIM_OFFSET:?FIM_OFFSET is unset or null}"
export FIM_SECTOR=$((FIM_OFFSET/512))

# Paths
export FIM_DIR_GLOBAL="${FIM_DIR_GLOBAL:?FIM_DIR_GLOBAL is unset or null}"
export FIM_DIR_GLOBAL_BIN="${FIM_DIR_GLOBAL}/bin"
export FIM_DIR_MOUNT="${FIM_DIR_MOUNT:?FIM_DIR_MOUNT is unset or null}"
export FIM_DIR_STATIC="$FIM_DIR_MOUNT/fim/static"
export FIM_FILE_CONFIG="$FIM_DIR_MOUNT/fim/config"
export FIM_DIR_TEMP="${FIM_DIR_TEMP:?FIM_DIR_TEMP is unset or null}"
export FIM_PATH_FILE_BINARY="${FIM_PATH_FILE_BINARY:?FIM_PATH_FILE_BINARY is unset or null}"
export FIM_FILE_BINARY="$(basename "$FIM_PATH_FILE_BINARY")"
export FIM_DIR_BINARY="$(dirname "$FIM_PATH_FILE_BINARY")"
export FIM_FILE_BASH="$FIM_DIR_GLOBAL_BIN/bash"
export BASHRC_FILE="$FIM_DIR_TEMP/.bashrc"
export FIM_FILE_PERMS="$FIM_DIR_MOUNT"/fim/perms

# Give static tools priority in PATH
export PATH="$FIM_DIR_GLOBAL_BIN:$PATH"

# Compression
export FIM_COMPRESSION_LEVEL="${FIM_COMPRESSION_LEVEL:-4}"
export FIM_SLACK_MINIMUM="${FIM_SLACK_MINIMUM:-50000000}" # 50MB
export FIM_COMPRESSION_DIRS="${FIM_COMPRESSION_DIRS:-/usr /opt}"

# Output stream
export FIM_STREAM="${FIM_DEBUG:+/dev/stdout}"
export FIM_STREAM="${FIM_STREAM:-/dev/null}"

# Check for stdout/stderr
if ! test -t 1 || ! test 2; then
  notify-send "Detected launch from GUI" || true
  exec 1> >(while IFS= read -r line; do echo "$line" | tee -a "${FIM_DIR_MOUNT}.log"; done)
  exec 2> >(while IFS= read -r line; do echo "$line" | tee -a "${FIM_DIR_MOUNT}.log" >&2; done)
fi

# Overlayfs filesystems mounts
declare -A FIM_MOUNTS_OVERLAYFS
declare -A FIM_MOUNTS_DWARFS

# shopt
shopt -s nullglob

# _msg() {{{
# Emits a message in &2
# $(1..n-1) arguments to echo
# $n message
function _msg()
{
  [ -z "$FIM_DEBUG" ] || echo -e "${@:1:${#@}-1}" "[*] ${*: -1}" >&2;
}
# }}}

# _wait_kill() {{{
# Wait for a pid to finish execution, similar to 'wait'
# but also works for non-child pids
# Kills on timeout
# $1: pid
function _wait_kill()
{
  # Get pid
  declare -i pid="$2"

  # Ignore pid 0, this might happen if pgrep
  # was done after process exit
  if test "$pid" -eq 0 || test -z "$pid"; then
    _msg "Pid $pid ignore..."
  fi

  # Wait message
  _msg "[ $pid ] : $*"
  # Get time limit
  declare -i limit="${3:-50}"

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

# _mount() {{{
# Mount the main filesystem
function _mount()
{
  local mode="${FIM_RW:-ro,}"
  local mode="${mode#1}"
  "$FIM_DIR_GLOBAL_BIN"/fuse2fs -o "$mode"fakeroot,offset="$FIM_OFFSET" "$FIM_PATH_FILE_BINARY" "$FIM_DIR_MOUNT" &> "$FIM_STREAM"

  if ! mount 2>&1 | grep "$FIM_DIR_MOUNT" &>/dev/null ; then
    echo "Could not mount main filesystem '$FIM_PATH_FILE_BINARY' to '$FIM_DIR_MOUNT'"
    kill "$PID"
  fi
}
# }}}

# _unmount() {{{
# Unmount the main filesystem
function _unmount()
{
  fusermount -zu "$FIM_DIR_MOUNT"

  for pid in $(pgrep -f "fuse2fs.*offset=$FIM_OFFSET.*$FIM_PATH_FILE_BINARY.*$FIM_DIR_MOUNT"); do
    _wait_kill "Wait for unmount of fuse2fs in $FIM_DIR_MOUNT" "$pid"
  done
}
# }}}

# _re_mount() {{{
# Re-mount the filesystem in new mountpoint
# $1 New mountpoint
function _re_mount()
{
  # Umount from initial mountpoint
  _unmount
  # Mount in new mountpoint
  export FIM_DIR_MOUNT="$1"; _mount
}
# }}}

# _die() {{{
# Quits the program
# $* = Termination message
function _die()
{
  # Force debug message
  [ -z "$*" ] || FIM_DEBUG=1 _msg "$*"
  # Exit
  kill -s SIGKILL "$PID"
}
trap _die SIGINT EXIT
# }}}

# _copy_tools() {{{
# Copies tools from fim static folder to host on /tmp
function _copy_tools()
{
  FIM_RO=1 FIM_RW="" _mount

  for i; do
    local tool="$i"

    if [ ! -f "$FIM_DIR_GLOBAL_BIN"/"$tool" ]; then
      cp "$FIM_DIR_MOUNT/fim/static/$tool" "$FIM_DIR_GLOBAL_BIN"
      chmod +x "$FIM_DIR_GLOBAL_BIN"/"$tool"
    fi
  done

  _unmount
}
# }}}

# _perms_list() {{{
# List permissions of sandbox
function _perms_list()
{
  ! grep -i "FIM_PERM_PULSEAUDIO" "$FIM_FILE_PERMS"  &>/dev/null || echo "pulseaudio"
  ! grep -i "FIM_PERM_WAYLAND" "$FIM_FILE_PERMS"     &>/dev/null || echo "wayland"
  ! grep -i "FIM_PERM_X11" "$FIM_FILE_PERMS"         &>/dev/null || echo "x11"
  ! grep -i "FIM_PERM_SESSION_BUS" "$FIM_FILE_PERMS" &>/dev/null || echo "session_bus"
  ! grep -i "FIM_PERM_SYSTEM_BUS" "$FIM_FILE_PERMS"  &>/dev/null || echo "system_bus"
  ! grep -i "FIM_PERM_GPU" "$FIM_FILE_PERMS"         &>/dev/null || echo "gpu"
  ! grep -i "FIM_PERM_INPUT" "$FIM_FILE_PERMS"       &>/dev/null || echo "input"
  ! grep -i "FIM_PERM_USB" "$FIM_FILE_PERMS"         &>/dev/null || echo "usb"
}
# }}}

# _perms_set() {{{
# Set permissions of sandbox
function _perms_set()
{
  # Reset perms
  echo "" > "$FIM_FILE_PERMS"

  # Set perms
  local ifs="$IFS" 
  IFS="," 
  #shellcheck disable=2016
  for i in $1; do
    case "$i" in
      pulseaudio)  echo 'FIM_PERM_PULSEAUDIO="${FIM_PERM_PULSEAUDIO:-1}"'   >> "$FIM_FILE_PERMS" ;;
      wayland)     echo 'FIM_PERM_WAYLAND="${FIM_PERM_WAYLAND:-1}"'         >> "$FIM_FILE_PERMS" ;;
      x11)         echo 'FIM_PERM_X11="${FIM_PERM_X11:-1}"'                 >> "$FIM_FILE_PERMS" ;;
      session_bus) echo 'FIM_PERM_SESSION_BUS="${FIM_PERM_SESSION_BUS:-1}"' >> "$FIM_FILE_PERMS" ;;
      system_bus)  echo 'FIM_PERM_SYSTEM_BUS="${FIM_PERM_SYSTEM_BUS:-1}"'   >> "$FIM_FILE_PERMS" ;;
      gpu)         echo 'FIM_PERM_GPU="${FIM_PERM_GPU:-1}"'                 >> "$FIM_FILE_PERMS" ;;
      input)       echo 'FIM_PERM_INPUT="${FIM_PERM_INPUT:-1}"'             >> "$FIM_FILE_PERMS" ;;
      usb)         echo 'FIM_PERM_USB="${FIM_PERM_USB:-1}"'                 >> "$FIM_FILE_PERMS" ;;
      *) _die "Trying to set unknown permission $i"
    esac
  done
  IFS="$ifs" 
}
# }}}

# _help() {{{
function _help()
{
  sed -E 's/^\s+://' <<-EOF
  :# FlatImage, $FIM_DIST
  :Avaliable options:
  :- fim-compress: Compress the filesystem to a read-only format.
  :- fim-root: Execute an arbitrary command as root.
  :- fim-exec: Execute an arbitrary command.
  :- fim-cmd: Set the default command to execute when no argument is passed.
  :- fim-resize: Resize the filesystem.
  :    - # Resizes the filesytem to 1G
  :    - E.g.: ./focal.fim fim-resize-free 1G
  :- fim-resize-free: Resize the filesystem to have the provided free space.
  :    - # Makes sure the filesystem has 100M of free space
  :    - E.g.: ./focal.fim fim-resize-free 100M
  :- fim-mount: Mount the filesystem in a specified directory
  :    - E.g.: ./focal.fim fim-mount ./mountpoint
  :- fim-xdg: Same as the 'fim-mount' command, however it opens the
  :    mount directory with xdg-open
  :- fim-perms-set: Set the permission for the container, available options are:
  :    pulseaudio, wayland, x11, session_bus, system_bus, gpu, input, usb
  :    - E.g.: ./focal.fim fim-perms pulseaudio,wayland,x11
  :- fim-perms-list: List the current permissions for the container
  :- fim-config-set: Sets a configuration that persists inside the image
  :    - E.g.: ./focal.fim fim-config-set home '"\$FIM_DIR_BINARY"/home.focal'
  :    - E.g.: ./focal.fim fim-config-set backend "proot"
  :- fim-config-list: List the current configurations for the container
  :    - E.g.: ./focal.fim fim-config-list                      # List all
  :    - E.g.: ./focal.fim fim-config-list "overlay.*"          # List ones that match regex
  :    - E.g.: ./focal.fim fim-config-list --single "overlay.*" # Stop on first match
  :    - E.g.: ./focal.fim fim-config-list --value  "overlay.*" # Print only the value
  :- fim-include-path: Includes a path inside the image, automatically resizing it in the process
  :    - E.g.: ./focal.fim fim-include-path ../my-dir /opt/new-folder1/new-folder2
  :    - E.g.: ./focal.fim fim-include-path ../my-file.tar /fim/tarballs
  :- fim-help: Print this message.
	EOF
}
# }}}

# _resize() {{{
# Changes the filesystem size
# $1 New size
function _resize()
{
  # Unmount
  _unmount

  # Resize
  "$FIM_DIR_GLOBAL_BIN"/e2fsck -fy "$FIM_PATH_FILE_BINARY"\?offset="$FIM_OFFSET" || true
  "$FIM_DIR_GLOBAL_BIN"/resize2fs "$FIM_PATH_FILE_BINARY"\?offset="$FIM_OFFSET" "$1"
  "$FIM_DIR_GLOBAL_BIN"/e2fsck -fy "$FIM_PATH_FILE_BINARY"\?offset="$FIM_OFFSET" || true

  # Mount
  _mount
}
# }}}

# _is_mounted {{{
# $1 Mountpoint
# Check if there is a mounted filesystem in mount point
function _is_mounted()
{
  df --output=target 2>/dev/null | grep -i "$1" &>/dev/null
}
# }}}

# _wait_for_mount {{{
function _wait_for_mount()
{
  declare -i i=0
  declare -i limit=50
  declare sec_sleep=0.1
  local mountpoint="$1"

  while ! _is_mounted "$mountpoint"; do
    _msg "Waiting for mount $mountpoint"
    sleep "$sec_sleep"
    i+=1
    if [[ "$i" -gt "$limit" ]]; then
      _die "Time limit reached for wait mount $mountpoint"
    fi
  done
}
# }}}

# _get_free_space {{{
# $1 Mountpoint of a mounted filesystem
# Returns the value in bytes
function _get_free_space()
{
  local mountpoint="$1"
  if ! _is_mounted "$mountpoint"; then
    _die "No filesystem mounted in '$mountpoint'"
  fi
  { df -B1 --output=avail "$mountpoint" 2>/dev/null || _die "df failed"; } | tail -n1 
}
# }}}

# _match_free_space() {{{
# The size of a filesystem and the free size of a filesystem differ
# This makes the free space on the filesystem match the target size
# $1 filesystem file
# $2 mountpoint
# $3 target size
# $4 filesystem offset
function _match_free_space()
{
  # Get filesystem file
  local file_filesystem="$1"

  # Get mountpoint
  local mountpoint="$2"
  mkdir -p "$mountpoint"

  # Get target size
  declare -i target="$(numfmt --from=iec "$3")"
  [[ "$target" =~ ^[0-9]+$ ]] || _die "target is NaN"

  # Optional offset
  declare -i offset
  if [[ $# -eq 4 ]]; then
    offset="$4"
  fi

  # Un-mount if mounted
  if _is_mounted "$mountpoint"; then
    _die "filesystem should not be mounted for _match_free_space"
  fi

  declare -i i=0
  while :; do
    ## Get current free size
    "$FIM_DIR_GLOBAL_BIN/fuse2fs" "${file_filesystem}" ${offset:+"-ooffset=$offset"} "$mountpoint"
    ## Wait for mount
    _wait_for_mount "$mountpoint"
    ## Grep free size
    declare -i curr_free="$(_get_free_space "$mountpoint")"
    ## Wait for mount process termination
    fusermount -u "$mountpoint"
    for pid in $(pgrep -f "fuse2fs.*$file_filesystem"); do
      _wait_kill "Wait for unmount of fuse2fs in $mountpoint" "$pid"
    done
    ## Check if got an integral number
    [[ "$curr_free" =~ ^[0-9]+$ ]] || _die "curr_free is NaN"
    _msg "Free space $(numfmt --to=iec "$curr_free")"

    # Check if has reached desired size
    if [[ "$curr_free" -gt "$target" ]]; then break; fi

    # Get the curr total
    declare -i curr_total="$(du -sb "$file_filesystem" | awk '{print $1}')"
    [[ "$curr_total" =~ ^[0-9]+$ ]] || _die "curr_total is NaN"

    # Increase on step
    declare -i step="$FIM_SLACK_MINIMUM"

    # Calculate new size
    declare -i new_size

    if [[ "$i" -eq 0 ]]; then
      i+=1
      new_size="$((curr_total + (target - curr_free) ))"
    else
      new_size="$((curr_total+step))"
    fi

    # Include size limit
    if [[ "$new_size" -gt "$(numfmt --from=iec "500G")" ]];  then _die "Too large filesystem resize attempt"; fi

    # Resize
    _msg "Target of $(numfmt --from=iec --to-unit=1M "$target")M"
    _msg "Resize from $(numfmt --from=iec --to-unit=1M "$curr_total")M to $(numfmt --from=iec --to-unit=1M "$new_size")M"

    "$FIM_DIR_GLOBAL_BIN/e2fsck" -fy "${file_filesystem}"${offset:+"?offset=$offset"} &> "$FIM_STREAM" || true
    "$FIM_DIR_GLOBAL_BIN/resize2fs" "${file_filesystem}${offset:+"?offset=$offset"}" \
      "$(numfmt --from=iec --to-unit=1Mi "${new_size}")M" &> "$FIM_STREAM"
    "$FIM_DIR_GLOBAL_BIN/e2fsck" -fy "${file_filesystem}"${offset:+"?offset=$offset"} &> "$FIM_STREAM" || true
  done
}
# }}}

# _resize_free_space() {{{
# Resizes filesystem to have at least the target free space
# If the input is less than the current free space, does nothing
# $1 New size for free space
function _resize_free_space()
{
  # Normalize to bytes
  declare -i size_bytes="$(numfmt --from=iec "$1")"

  # Un-mount
  _unmount

  # Match free space
  _match_free_space "$FIM_PATH_FILE_BINARY" "$FIM_DIR_MOUNT" "$size_bytes" "$FIM_OFFSET"

  # Re-mount
  _mount
}
# }}}

# _include_path {{{ 
# $1 Path to the file/directory to include
# $2 Path to the directory to include it into
function _include_path()
{
  # Input file/dir
  local path_target="$1"

  # Verify
  if ! [[ -d "$path_target" ]] && ! [[ -f "$path_target" ]]; then
    _die "File '$path_target' does not exist or is invalid"
  fi
  if df --output=target "$path_target" 2>/dev/null | grep -i "$FIM_DIR_MOUNT" &>/dev/null; then
    _die "Target cannot not be inside the guest filesystem"
  fi

  # Check if exists
  local dir_guest="$FIM_DIR_MOUNT/$2"
  if [[ -e "$dir_guest" ]]; then
    _die "Directory '$2' already exists inside the flatimage (or is a file), remove it beforehand"
  fi

  # Get size of target to include
  local size_target="$(du -sb "$path_target" | awk '{print $1}')"
  [[ "$size_target" =~ ^[0-9]+$ ]] || _die "size_target is NaN: '$size_target'"

  # Get currently free size
  local size_free="$(_get_free_space "$FIM_DIR_MOUNT")"
  _resize_free_space "$((size_free+size_target))"

  FIM_DEBUG=1 _msg "Include target '$path_target' in '$dir_guest'"
  cp -r "$path_target" "$dir_guest"
}
# }}}

# _rebuild() {{{
# Re-create the filesystem with new data
# $1 New size in bytes
# $2 Dir to create image from
function _rebuild()
{
  _unmount

  declare -i size_image="$1"
  local dir_system="$2"
  local file_image="$FIM_DIR_BINARY/image.fim"

  # Erase current file
  rm "$FIM_PATH_FILE_BINARY"

  # Copy startup binary
  cp "$FIM_DIR_TEMP/main" "$FIM_PATH_FILE_BINARY"

  # Append tools
  cat "$FIM_DIR_GLOBAL_BIN"/{fuse2fs,e2fsck,bash}  >> "$FIM_PATH_FILE_BINARY"

  # Update offset
  FIM_OFFSET="$(du -sb "$FIM_PATH_FILE_BINARY" | awk '{print $1}')"

  # Create filesystem
  truncate -s "$size_image" "$file_image"

  # Check block size of host
  local block_size="$(stat -fc %s .)"
  _msg "block size: $block_size"

  # Format as ext2
  "$FIM_DIR_GLOBAL_BIN"/mke2fs -F -b"$(stat -fc %s .)" -t ext2 "$file_image" &> "$FIM_STREAM"

  # Make sure enough free space is available
  _match_free_space "$file_image" "$FIM_DIR_BINARY/${FIM_FILE_BINARY}.mount" "$size_image"

  # Re-format and include files
  "$FIM_DIR_GLOBAL_BIN"/mke2fs -F -d "$dir_system" -b"$(stat -fc %s .)" -t ext2 "$file_image" &> "$FIM_STREAM"

  # Append filesystem to binary
  cat "$file_image" >> "$FIM_PATH_FILE_BINARY"

  # Remove filesystem
  rm "$file_image"

  # Re-mount
  _mount
}
# }}}

# _find_dwarfs() {{{
# Finds the mountpoints for the dwarfs filesystems
# Populates FIM_MOUNTS_DWARFS (filesystem file -> mountpoint)
function _find_dwarfs()
{
  while read -r i; do
    local filesystem_file="$i"
    # Define mountpoint
    local mountpoint="$FIM_DIR_GLOBAL/dwarfs/$DWARFS_SHA/$(basename "$i")"
    mountpoint="${mountpoint%.dwarfs}"
    # Log
    _msg "DWARFS FS: $filesystem_file"
    _msg "DWARFS MP: $mountpoint"
    # Save
    FIM_MOUNTS_DWARFS["$filesystem_file"]="$mountpoint"
  done < <(find "$FIM_DIR_MOUNT" -maxdepth 1 -iname "*.dwarfs")
}
# }}}

# _find_overlayfs() {{{
# Finds the mountpoints for the overlayfs filesystems
# Populates FIM_MOUNTS_OVERLAYFS (src dir -> target dir)
function _find_overlayfs()
{
  while read -r overlay; do
    # Fetch paths
    local bind_cont="$(_config_fetch --single --value "${overlay}.cont")"
    local bind_host="$(_config_fetch --single --value "${overlay}.host")"
    # Check empty string
    [[ -n "$bind_cont" ]] || { FIM_DEBUG=1 _msg "You must set bind_cont for $overlay"; break; }
    [[ -n "$bind_host" ]] || { FIM_DEBUG=1 _msg "You must set bind_host for $overlay"; break; }
    # Expand
    bind_cont="$(eval echo "$bind_cont")"
    bind_host="$(eval echo "$bind_host")" 
    # Adjust path to relative from inside the container
    bind_cont="$FIM_DIR_MOUNT/$bind_cont"
    # Log
    _msg "OVERLAYFS SRC: ${bind_cont}"
    _msg "OVERLAYFS DST: ${bind_host}"
    # Save
    FIM_MOUNTS_OVERLAYFS["$bind_cont"]="$bind_host"
  done < <(_config_fetch --key "^overlay\.[A-Za-z0-9_]+ =")
}
# }}}

# _mount_dwarfs() {{{
# # Mount dwarfs filesystems defined in FIM_MOUNTS_DWARFS
function _mount_dwarfs()
{
  ## Mount dwarfs files if exist
  # shellcheck disable=2044
  for i in "${!FIM_MOUNTS_DWARFS[@]}"; do
    local fs="$i"
    local mp="${FIM_MOUNTS_DWARFS["$i"]}"
    # Create mountpoint
    mkdir -p "$mp"
    # Symlink, skip if directory exists
    ln -T -sfn "$mp" "${fs%.dwarfs}" || continue
    # Mount
    "$FIM_DIR_GLOBAL_BIN/dwarfs" "$fs" "$mp" &> "$FIM_STREAM"
  done
}
# }}}

# _mount_overlayfs() {{{
# # Mount overlayfs filesystems defined in FIM_MOUNTS_OVERLAYFS
function _mount_overlayfs()
{
  # Mount overlayfs
  ## Read overlays
  for i in "${!FIM_MOUNTS_OVERLAYFS[@]}"; do
    # Fetch paths
    local bind_cont="$i"
    local bind_host="${FIM_MOUNTS_OVERLAYFS[$i]}"
    # Test if target exists
    if ! test -e "$bind_cont"; then
      _msg "Target of overlay '$overlay', $bind_cont, does not exist"
    fi
    # Define host-sided paths
    local workdir="$bind_host"/workdir
    local upperdir="$bind_host"/upperdir
    local mount="$bind_host"/mount
    _msg "OVERLAYFS workdir: $workdir"
    _msg "OVERLAYFS upperdir: $upperdir"
    _msg "OVERLAYFS mount: $mount"
    # Create host-sided paths
    mkdir -pv "$bind_host"/{workdir,upperdir,mount} &> "$FIM_STREAM"
    # If is a symlink from inside the container that points to a directory in the host
    # # Update bind_cont with resolved path (might be a symlink created by dwarfs)
    # # Replace symlink to point to mount to overlayfs
    if test -L "$bind_cont"; then
      local symm="$bind_cont"
      _msg "OVERLAYFS unresolved: $symm"
      bind_cont="$(readlink -f "$bind_cont")"
      _msg "OVERLAYFS lowerdir: $bind_cont"
      ln -T -sfn "$mount" "$symm"
    else
      FIM_DEBUG=1 _msg "Overlay container mount '$bind_cont' not a symlink"
      break
    fi
    # Define lowerdir
    local lowerdir="$bind_cont"
    # Mount
    overlayfs -o squash_to_uid=1000,lowerdir="$lowerdir",upperdir="$upperdir",workdir="$workdir" "$mount"
  done
}
# }}}

# _setup_filesystems() {{{
function _setup_filesystems()
{
  # Mount dwarfs files
  ## Fetch SHA
  export DWARFS_SHA="$(_config_fetch --value --single "sha")"
  _msg "DWARFS_SHA: $DWARFS_SHA"

  ## Copy dwarfs binary
  [ -f "$FIM_DIR_GLOBAL_BIN/dwarfs" ]  || cp "$FIM_DIR_MOUNT/fim/static/dwarfs" "$FIM_DIR_GLOBAL_BIN"/dwarfs
  chmod +x "$FIM_DIR_GLOBAL_BIN/dwarfs"

  # Find mountpoints
  _find_dwarfs
  _find_overlayfs

  # Save dwarfs mountpoints
  for i in "${FIM_MOUNTS_DWARFS[@]}"; do
    echo "$i" >> "${FIM_DIR_MOUNT}.dwarfs.dst"
  done

  # Save overlayfs mountpoints
  for i in "${FIM_MOUNTS_OVERLAYFS[@]}"; do
    echo "$i" >> "${FIM_DIR_MOUNT}.overlayfs.dst"
  done

  # Mount filesystems
  _mount_dwarfs
  _mount_overlayfs
}
# }}}

# _exec() {{{
# Chroots into the filesystem
# $* Command and args
function _exec()
{
  # Check for empty string
  [ -n "$*" ] || FIM_DEBUG=1 _msg "Empty arguments for exec"

  # Mount overlayfs and dwarfs
  _setup_filesystems

  # Fetch CMD
  declare -a cmd
  for i; do
    [ -z "$i" ] || cmd+=("\"$i\"")
  done

  _msg "cmd: ${cmd[*]}"

  # Export variables to container
  export TERM="xterm"
  if [[ -z "$XDG_RUNTIME_DIR" ]] && [[ -e "/run/user/$(id -u)" ]]; then
    export XDG_RUNTIME_DIR="/run/user/$(id -u)"
  fi
  export HOST_USERNAME="$(whoami)"
  export PATH="$PATH:/sbin:/usr/sbin:/usr/local/sbin:/bin:/usr/bin:/usr/local/bin"
  tee "$BASHRC_FILE" &>/dev/null <<- 'EOF'
    export PS1="(flatimage@"${FIM_DIST,,}") → "
	EOF

  # Remove override to avoid problems with apt
  [ -n "$FIM_RO" ] || rm ${FIM_DEBUG:+-v} -f "$FIM_DIR_MOUNT/var/lib/dpkg/statoverride"

  declare -a _cmd_bwrap
  declare -a _cmd_proot

  # Fetch permissions
  # shellcheck disable=1090
  source "$FIM_FILE_PERMS"

  # Main binary
  _cmd_bwrap+=("$FIM_DIR_STATIC/bwrap")
  _cmd_proot+=("$FIM_DIR_STATIC/proot")

  # Root binding
  _cmd_bwrap+=("${FIM_ROOT:+--uid 0 --gid 0}")
  _cmd_proot+=("-0")

  # Path to subsystem
  _cmd_bwrap+=("--bind \"$FIM_DIR_MOUNT\" /")
  _cmd_proot+=("-r \"$FIM_DIR_MOUNT\"")

  # Set mount bindings for external media
  for i in "/media" "/run/media" "/mnt"; do
    if [ -d "$i" ]; then
      _msg "BIND: $i"
      _cmd_bwrap+=("--ro-bind \"$i\" \"$i\"")
      _cmd_proot+=("-b \"$i\"")
    fi
  done

  # System bindings
  ## bwrap
  _cmd_bwrap+=("--dev /dev")
  _cmd_bwrap+=("--proc /proc")
  _cmd_bwrap+=("--bind /tmp /tmp")
  _cmd_bwrap+=("--bind /sys /sys")
  ## proot
  _cmd_proot+=("-b /dev")
  _cmd_proot+=("-b /proc")
  _cmd_proot+=("-b /tmp")
  _cmd_proot+=("-b /sys")

  # Create HOME if not exists
  mkdir -p "$HOME"

  # Set home binding
  if [ "$HOME" != "/home/$(whoami)" ] && [ "$(id -u)" != "0" ]; then
    _msg "HOME_BIND: Nested"
    # Host home
    _cmd_bwrap+=("--ro-bind \"/home/$(whoami)\" \"/home/$(whoami)\"")
    _cmd_proot+=("-b \"/home/$(whoami)\"")
    # User home
    _cmd_bwrap+=("--bind \"$HOME\" \"$HOME\"")
    _cmd_proot+=("-b \"$HOME\"")
  else
    _msg "HOME_BIND: Host"
    # User home
    _cmd_bwrap+=("--bind \"$HOME\" \"$HOME\"")
    _cmd_proot+=("-b \"$HOME\"")
  fi

  # Pulseaudio
  if [[ "$FIM_PERM_PULSEAUDIO" -eq 1 ]] &&
     [[ -n "$XDG_RUNTIME_DIR" ]]; then
    _msg "PERM: Pulseaudio"
    local PULSE_SOCKET="$XDG_RUNTIME_DIR/pulse/native"
    export PULSE_SERVER="unix:$PULSE_SOCKET"
    # bwrap
    _cmd_bwrap+=("--setenv PULSE_SERVER unix:$PULSE_SOCKET")
    _cmd_bwrap+=("--bind $PULSE_SOCKET $PULSE_SOCKET")
    # proot
    _cmd_proot+=("-b $PULSE_SOCKET")
  fi

  # Wayland
  if [[ "$FIM_PERM_WAYLAND" -eq 1 ]] &&
     [[ -n "$XDG_RUNTIME_DIR" ]] &&
     [[ -n "$WAYLAND_DISPLAY" ]]; then
    _msg "PERM: Wayland"
    local WAYLAND_SOCKET_PATH="$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY"
    export WAYLAND_DISPLAY="$WAYLAND_DISPLAY"
    export XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR"
    ## bwrap
    _cmd_bwrap+=("--bind $WAYLAND_SOCKET_PATH $WAYLAND_SOCKET_PATH")
    _cmd_bwrap+=("--setenv WAYLAND_DISPLAY $WAYLAND_DISPLAY")
    _cmd_bwrap+=("--setenv XDG_RUNTIME_DIR $XDG_RUNTIME_DIR")
    ## proot
    _cmd_proot+=("-b $WAYLAND_SOCKET_PATH")
  fi

  # X11
  if [[ "$FIM_PERM_X11" -eq 1 ]] &&
     [[ -n "$DISPLAY" ]] &&
     [[ -n "$XAUTHORITY" ]]; then
    _msg "PERM: X11"
    export DISPLAY="$DISPLAY"
    export XAUTHORITY="$XAUTHORITY"
    ## bwrap
    _cmd_bwrap+=("--setenv DISPLAY $DISPLAY")
    _cmd_bwrap+=("--setenv XAUTHORITY $XAUTHORITY")
    _cmd_bwrap+=("--ro-bind $XAUTHORITY $XAUTHORITY")
    ## proot
    _cmd_proot+=("-b $XAUTHORITY")
  fi

  # dbus (user)
  if [[ "$FIM_PERM_SESSION_BUS" -eq 1 ]] &&
     [[ -n "$DBUS_SESSION_BUS_ADDRESS" ]]; then
    _msg "PERM: SESSION BUS"
    local dbus_session_bus_path="${DBUS_SESSION_BUS_ADDRESS#*=}"
    dbus_session_bus_path="${dbus_session_bus_path%%,*}"
    export DBUS_SESSION_BUS_ADDRESS
    ## bwrap
    _cmd_bwrap+=("--setenv DBUS_SESSION_BUS_ADDRESS $DBUS_SESSION_BUS_ADDRESS")
    _cmd_bwrap+=("--bind $dbus_session_bus_path $dbus_session_bus_path")
    ## proot
    _cmd_proot+=("-b ${DBUS_SESSION_BUS_ADDRESS#*=}")
  fi

  # dbus (system)
  if [[ "$FIM_PERM_SYSTEM_BUS" -eq 1 ]] &&
     [[ -e "/run/dbus/system_bus_socket" ]]; then
    _msg "PERM: SYSTEM BUS"
    ## bwrap
    _cmd_bwrap+=("--bind /run/dbus/system_bus_socket /run/dbus/system_bus_socket")
    ## proot
    _cmd_proot+=("-b /run/dbus/system_bus_socket")
  fi

  # GPU
  if [[ "$FIM_PERM_GPU" -eq 1 ]] &&
     [[ -e "/dev/dri" ]]; then
    _msg "PERM: GPU"

    _cmd_bwrap+=("--dev-bind /dev/dri /dev/dri")
    _cmd_proot+=("-b /dev/dri")

    if { lsmod | grep -i nvidia; } &>/dev/null; then
      _msg "Nvidia GPU detected, setting up driver bindings..."
      ### Bind devices
      for i in /dev/*nvidia*; do
        _cmd_bwrap+=("--dev-bind \"$i\" \"$i\"")
        _cmd_proot+=("-b \"$i\"")
      done &>"$FIM_STREAM" || true
      ### Bind files
      declare -a nvidia_binds
      nvidia_binds+=(/usr/lib/*nvidia*)
      nvidia_binds+=(/usr/lib/*cuda*)
      nvidia_binds+=(/usr/lib/*nvcuvid*)
      nvidia_binds+=(/usr/lib/*nvoptix*)
      nvidia_binds+=(/usr/lib/*vdpau*)
      nvidia_binds+=(/usr/lib/x86_64-linux-gnu/*nvidia*)
      nvidia_binds+=(/usr/lib/x86_64-linux-gnu/*cuda*)
      nvidia_binds+=(/usr/lib/x86_64-linux-gnu/*nvcuvid*)
      nvidia_binds+=(/usr/lib/x86_64-linux-gnu/*nvoptix*)
      nvidia_binds+=(/usr/lib/x86_64-linux-gnu/*vdpau*)
      nvidia_binds+=(/usr/lib/i386-linux-gnu/*nvidia*)
      nvidia_binds+=(/usr/lib/i386-linux-gnu/*cuda*)
      nvidia_binds+=(/usr/lib/i386-linux-gnu/*nvcuvid*)
      nvidia_binds+=(/usr/lib/i386-linux-gnu/*nvoptix*)
      nvidia_binds+=(/usr/lib/i386-linux-gnu/*vdpau*)
      nvidia_binds+=(/usr/bin/*nvidia*)
      nvidia_binds+=(/usr/share/*nvidia*)
      nvidia_binds+=(/usr/share/vulkan/icd.d/*nvidia*)
      nvidia_binds+=(/usr/lib32/*nvidia*)
      nvidia_binds+=(/usr/lib32/*cuda*)
      nvidia_binds+=(/usr/lib32/*vdpau*)

      ### Set library search paths
      export LD_LIBRARY_PATH="/usr/lib/x86_64-linux-gnu:/usr/lib/i386-linux-gnu:$LD_LIBRARY_PATH"

      ### Find actual root location if it a dwarfs or overlayfs symlink
      local dir_usr="$(readlink -f "$FIM_DIR_MOUNT/usr")"

      ### Bind
      for i in "${nvidia_binds[@]}"; do
        _cmd_bwrap+=("--bind \"$i\" \"${dir_usr}/${i#/usr}\"")
        _cmd_proot+=("-b \"$i\"")
        _msg "NVIDIA bind '$i'"
      done &>"$FIM_STREAM" || true
    fi
  fi

  # Input
  if [[ "$FIM_PERM_INPUT" -eq 1 ]] &&
     [[ -e "/dev/input" ]]; then
    _msg "PERM: Input - /dev/input"
    ## bwrap
    _cmd_bwrap+=("--dev-bind /dev/input /dev/input")
    ## proot
    _cmd_proot+=("-b /dev/input /dev/input")
  fi
  if [[ "$FIM_PERM_INPUT" -eq 1 ]] &&
     [[ -e "/dev/uinput" ]]; then
    _msg "PERM: Input - /dev/uinput"
    ## bwrap
    _cmd_bwrap+=("--dev-bind /dev/uinput /dev/uinput")
    ## proot
    _cmd_proot+=("-b /dev/uinput /dev/uinput")
  fi

  # USB
  if [[ "$FIM_PERM_USB" -eq 1 ]] &&
     [[ -e "/dev/bus/usb" ]]; then
    _msg "PERM: USB - /dev/bus/usb"
    ## bwrap
    _cmd_bwrap+=("--dev-bind /dev/bus/usb /dev/bus/usb")
    ## proot
    _cmd_proot+=("-b /dev/bus/usb /dev/bus/usb")
  fi
  if [[ "$FIM_PERM_USB" -eq 1 ]] &&
     [[ -e "/dev/usb" ]]; then
    _msg "PERM: USB - /dev/usb"
    ## bwrap
    _cmd_bwrap+=("--dev-bind /dev/usb /dev/usb")
    ## proot
    _cmd_proot+=("-b /dev/usb /dev/usb")
  fi

  # Host info
  ## bwrap
  [ ! -f "/etc/host.conf"     ] || _cmd_bwrap+=('--bind "/etc/host.conf"     "/etc/host.conf"')
  [ ! -f "/etc/hosts"         ] || _cmd_bwrap+=('--bind "/etc/hosts"         "/etc/hosts"')
  # [ ! -f "/etc/passwd"        ] || _cmd_bwrap+=('--bind "/etc/passwd"        "/etc/passwd"')
  [ ! -f "/etc/group"         ] || _cmd_bwrap+=('--bind "/etc/group"         "/etc/group"')
  [ ! -f "/etc/nsswitch.conf" ] || _cmd_bwrap+=('--bind "/etc/nsswitch.conf" "/etc/nsswitch.conf"')
  [ ! -f "/etc/resolv.conf"   ] || _cmd_bwrap+=('--bind "/etc/resolv.conf"   "/etc/resolv.conf"')
  ## proot
  [ ! -f "/etc/host.conf"     ] || _cmd_proot+=('-b "/etc/host.conf"')
  [ ! -f "/etc/hosts"         ] || _cmd_proot+=('-b "/etc/hosts"')
  [ ! -f "/etc/passwd"        ] || _cmd_proot+=('-b "/etc/passwd"')
  [ ! -f "/etc/group"         ] || _cmd_proot+=('-b "/etc/group"')
  [ ! -f "/etc/nsswitch.conf" ] || _cmd_proot+=('-b "/etc/nsswitch.conf"')
  [ ! -f "/etc/resolv.conf"   ] || _cmd_proot+=('-b "/etc/resolv.conf"')

  # Shell
  _cmd_bwrap+=("$FIM_FILE_BASH -c '${cmd[*]}'")
  _cmd_proot+=("$FIM_FILE_BASH -c '${cmd[*]}'")

  # Run in contained environment
  if [[ "$FIM_BACKEND" = "bwrap" ]]; then
    _msg "Using bubblewrap"
    eval "${_cmd_bwrap[*]}"
  elif [[ "$FIM_BACKEND" = "proot" ]]; then
    _msg "Using proot"
    eval "${_cmd_proot[*]}"
  # Run outside container
  elif [[ "$FIM_BACKEND" = "host" ]]; then
    "$FIM_FILE_BASH" -c "${cmd[*]}"
  else
    _die "Invalid backend $FIM_BACKEND"
  fi

}
# }}}

# _compress() {{{
# Subdirectory compression
function _compress()
{
  [ -n "$FIM_RW" ] || _die "Set FIM_RW to 1 before compression"
  [ -z "$(_config_fetch --value --single "sha")" ] || _die "sha is set (already compressed?)"

  # Copy compressor to binary dir
  [ -f "$FIM_DIR_GLOBAL_BIN/mkdwarfs" ]  || cp "$FIM_DIR_MOUNT/fim/static/mkdwarfs" "$FIM_DIR_GLOBAL_BIN"/mkdwarfs
  chmod +x "$FIM_DIR_GLOBAL_BIN/mkdwarfs"

  # Remove apt lists and cache
  rm -rf "$FIM_DIR_MOUNT"/var/{lib/apt/lists,cache}

  # Create temporary directory to fit-resize fs
  local dir_compressed="$FIM_DIR_BINARY/$FIM_FILE_BINARY.tmp"
  rm -rf "$dir_compressed"
  mkdir "$dir_compressed"

  # Get SHA and save to re-mount (used as unique identifier)
  local sha="$(sha256sum "$FIM_PATH_FILE_BINARY" | awk '{print $1}')"
  _config_set "sha" "$sha"
  _msg "sha: $sha"

  # Compress selected directories
  for i in ${FIM_COMPRESSION_DIRS}; do
    local target="$FIM_DIR_MOUNT/$i"
    [ -d "$target" ] ||  _die "Folder $target not found for compression"
    "$FIM_DIR_GLOBAL_BIN/mkdwarfs" -i "$target" -o "${dir_compressed}/$i.dwarfs" -l"$FIM_COMPRESSION_LEVEL" -f
    rm -rf "$target"
    ln -sf "$FIM_DIR_GLOBAL/dwarfs/$sha/$i" "${dir_compressed}/${i}"
  done


  # Remove remaining files from dev
  rm -rf "${FIM_DIR_MOUNT:?"Empty FIM_DIR_MOUNT"}"/dev

  # Move files to temporary directory
  for i in "$FIM_DIR_MOUNT"/{fim,bin,etc,lib,lib64,opt,root,run,sbin,share,tmp,usr,var}; do
    { mv "$i" "$dir_compressed" || true; } &>"$FIM_STREAM"
  done

  # Update permissions
  chmod -R +rw "$dir_compressed"

  # Resize to fit files size + slack
  local size_files="$(du -sb "$dir_compressed" | awk '{print $1}')"
  local size_slack="$FIM_SLACK_MINIMUM";
  local size_new="$((size_files+size_slack))"

  _msg "Size files        : $size_files"
  _msg "Size slack        : $size_slack"
  _msg "Size sum          : $size_new"

  # Resize
  _rebuild "$size_new" "$dir_compressed"

  # Remove mount dirs
  rm -rf "${FIM_DIR_MOUNT:?"Empty mount var"}"/{tmp,proc,sys,dev,run}

  # Create required mount points if not exists
  mkdir -p "$FIM_DIR_MOUNT"/{tmp,proc,sys,dev,run,home}
}
# }}}

# _config_fetch() {{{
# Fetches a configuration from $FIM_FILE_CONFIG
# --single: Quits on first match
# --value: Only prints the value
# $*: A regex, empty matches all
function _config_fetch()
{
  [ -f "$FIM_FILE_CONFIG" ] || exit

  # Exit on first match
  declare -i single=0
  # Print only value
  declare -i value=0
  # Print only key
  declare -i key=0

  # Parse args
  while :; do
    case "$1" in
      --single) single=1; shift ;;
      --value) value=1; shift ;;
      --key) key=1; shift ;;
      *) break
    esac
  done

  # Remainder of args is regex
  local regex="$*"
  # Match all on empty query
  local regex="${regex:-".*"}"

  # List ones that match regex
  while read -r i; do
    if [[ -z "$i" ]]; then continue; fi
    if [[ "$i" =~ $regex ]]; then
      # Print value or entire expression
      if [[ "$value" -eq 1 ]]; then
        [[ "$i" =~ ([^=]*)=(.*) ]] && echo "${BASH_REMATCH[2]}" | xargs
      elif [[ "$key" -eq 1 ]]; then
        [[ "$i" =~ ([^=]*)=(.*) ]] && echo "${BASH_REMATCH[1]}" | xargs
      else
        echo "$i" | xargs
      fi
      if [[ "$single" -eq 1 ]]; then break; fi
    fi
  done < "$FIM_FILE_CONFIG"
}
# }}}

# _config_set() {{{
function _config_set()
{
  local opt="$1"; shift
  local entry="$opt = $*"

  if grep "$opt =" "$FIM_FILE_CONFIG" &>"$FIM_STREAM"; then
    sed -i "s|$opt =.*|$entry|" "$FIM_FILE_CONFIG"
  else
    echo "$entry" >> "$FIM_FILE_CONFIG"
  fi
}
# }}}

# main() {{{
function main()
{
  _msg "FIM_OFFSET           : $FIM_OFFSET"
  _msg "FIM_RO               : $FIM_RO"
  _msg "FIM_RW               : $FIM_RW"
  _msg "FIM_STREAM           : $FIM_STREAM"
  _msg "FIM_ROOT             : $FIM_ROOT"
  _msg "FIM_NORM             : $FIM_NORM"
  _msg "FIM_DEBUG            : $FIM_DEBUG"
  _msg "FIM_NDEBUG           : $FIM_NDEBUG"
  _msg "FIM_DIR_GLOBAL       : $FIM_DIR_GLOBAL"
  _msg "FIM_DIR_GLOBAL_BIN   : $FIM_DIR_GLOBAL_BIN"
  _msg "FIM_DIR_MOUNT        : $FIM_DIR_MOUNT"
  _msg "FIM_DIR_TEMP         : $FIM_DIR_TEMP"
  _msg "FIM_PATH_FILE_BINARY : $FIM_PATH_FILE_BINARY"
  _msg "FIM_FILE_BINARY      : $FIM_FILE_BINARY"
  _msg "FIM_DIR_BINARY       : $FIM_DIR_BINARY"
  _msg '$*                   : '"$*"

  # Check filesystem
  "$FIM_DIR_GLOBAL_BIN"/e2fsck -fy "$FIM_PATH_FILE_BINARY"\?offset="$FIM_OFFSET" &> "$FIM_STREAM" || true

  # Copy tools
  declare -a ext_tools=(
    "[" "b2sum" "base32" "base64" "basename" "cat" "chcon" "chgrp" "chmod" "chown" "chroot" "cksum"
    "comm" "cp" "csplit" "cut" "date" "dcgen" "dd" "df" "dir" "dircolors" "dirname" "du" "echo" "env"
    "expand" "expr" "factor" "false" "fmt" "fold" "groups" "head" "hostid" "id" "join" "kill" "link"
    "ln" "logname" "ls" "md5sum" "mkdir" "mkfifo" "mknod" "mktemp" "mv" "nice" "nl" "nohup" "nproc"
    "numfmt" "od" "paste" "pathchk" "pr" "printenv" "printf" "ptx" "pwd" "readlink" "realpath" "rm"
    "rmdir" "runcon" "seq" "sha1sum" "sha224sum" "sha256sum" "sha384sum" "sha512sum" "shred" "shuf"
    "sleep" "sort" "split" "stat" "stty" "sum" "sync" "tac" "tail" "tee" "test" "timeout" "touch" "tr"
    "true" "truncate" "tsort" "uname" "unexpand" "uniq" "unlink" "uptime" "users" "vdir" "wc" "who"
    "whoami" "yes" "resize2fs" "mke2fs" "lsof"
  )

  # Setup daemon to unmount filesystems on exit
  chmod +x "$FIM_FPATH_KILLER"
  nohup "$FIM_FPATH_KILLER" &>/dev/null & disown

  # Copy static binaries
  _copy_tools "${ext_tools[@]}"

  # Mount filesystem
  _mount

  # Resize by offset if space is less than minimum
  _resize_free_space "$FIM_SLACK_MINIMUM"

  # Check if config exists, else try to touch if mounted as RW
  [ -f "$FIM_FILE_CONFIG" ] || { [ -n "$FIM_RO" ] || touch "$FIM_FILE_CONFIG"; }

  # Check if custom home directory is set
  local home="$(_config_fetch --value --single "home")"
  # # Expand
  home="$(eval echo "$home")"
  # # Set & show on debug mode
  [[ -z "$home" ]] || { mkdir -p "$home" && export HOME="$home"; }
  _msg "FIM_HOME        : $HOME"

  # If FIM_BACKEND is not defined check the config
  # or set it to bwrap
  if [[ -z "$FIM_BACKEND" ]]; then
    local fim_tool="$(_config_fetch --value --single "backend")"
    if [[ -n "$fim_tool" ]]; then
      FIM_BACKEND="$fim_tool"
    else
      FIM_BACKEND="bwrap"
    fi
  fi

  if [[ "${1:-}" =~ fim-(.*) ]]; then
    case "${BASH_REMATCH[1]}" in
      "compress") _compress ;;
      "root") FIM_ROOT=1; FIM_NORM="" ;&
      "exec") shift; _exec "$@" ;;
      "cmd") _config_set "cmd" "${@:2}" ;;
      "resize") _resize "$2" ;;
      "resize-free") _resize_free_space "$2" ;;
      "xdg") _re_mount "$2"; xdg-open "$2"; read -r ;;
      "mount") _re_mount "$2"; read -r ;;
      "config-list") shift; _config_fetch "$*" ;;
      "config-set") _config_set "$2" "$3";;
      "perms-list") _perms_list ;;
      "perms-set") _perms_set "$2";;
      "include-path") _include_path "$2" "$3" ;;
      "help") _help;;
      *) _help; _die "Unknown fim command" ;;
    esac
  else
    local default_cmd="$(_config_fetch --value --single "cmd")"
    _exec  "${default_cmd:-"$FIM_FILE_BASH"}" "$@"
  fi

}
# }}}

main "$@"

#  vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :
