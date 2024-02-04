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

export FIM_DIST="TRUNK"

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
export FIM_FILE_BINARY="${FIM_FILE_BINARY:?FIM_FILE_BINARY is unset or null}"
export FIM_BASENAME_BINARY="$(basename "$FIM_FILE_BINARY")"
export FIM_DIR_BINARY="$(dirname "$FIM_FILE_BINARY")"
export FIM_FILE_BASH="$FIM_DIR_GLOBAL_BIN/bash"
export BASHRC_FILE="$FIM_DIR_TEMP/.bashrc"
export FIM_FILE_PERMS="$FIM_DIR_MOUNT"/fim/perms
export FIM_DIR_DWARFS="$FIM_DIR_MOUNT/fim/dwarfs"
export FIM_DIR_HOOKS="$FIM_DIR_MOUNT/fim/hooks"

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
  notify-send "Using custom stdout/stderr to ${FIM_DIR_MOUNT}.log" &>"$FIM_STREAM" || true
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
  [ -z "$FIM_DEBUG" ] || echo -e "${@:1:${#@}-1}" "[:${FUNCNAME[1]}:] ${*: -1}" >&2;
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

# _mount() {{{
# Mount the main filesystem
function _mount()
{
  local mode="${FIM_RW:-ro,}"
  local mode="${mode#1}"
  "$FIM_DIR_GLOBAL_BIN"/fuse2fs -o "$mode"fakeroot,offset="$FIM_OFFSET" "$FIM_FILE_BINARY" "$FIM_DIR_MOUNT" &> "$FIM_STREAM"

  if ! mount 2>&1 | grep "$FIM_DIR_MOUNT" &>/dev/null ; then
    echo "Could not mount main filesystem '$FIM_FILE_BINARY' to '$FIM_DIR_MOUNT'"
    kill "$PID"
  fi
}
# }}}

# _unmount() {{{
# Unmount the main filesystem
function _unmount()
{
  fusermount -zu "$FIM_DIR_MOUNT"

  for pid in $(pgrep -f "fuse2fs.*offset=$FIM_OFFSET.*$FIM_FILE_BINARY.*$FIM_DIR_MOUNT"); do
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
  # Signal exit to killer
  echo 1 > "${FIM_DIR_MOUNT}.killer.kill"
  # Wait_kill self
  _wait_kill "Wait for self to finish" "$PID" 600
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
  echo "FlatImage, $FIM_DIST"

  sed -E 's/^\s+://' <<-"EOF"
  :Avaliable options:
  :- fim-compress: Compress the filesystem to a read-only format.
  :- fim-root: Execute an arbitrary command as root.
  :- fim-exec: Execute an arbitrary command.
  :- fim-cmd: Set the default command to execute when no argument is passed.
  :- fim-resize: Resizes the filesystem.
  :    - # Resizes the filesytem to have 1G of size
  :    - E.g.: ./arch.flatimage fim-resize 1G
  :    - # Resizes the filesystem by current size plus 1G
  :    - E.g.: ./arch.flatimage fim-resize +1G
  :- fim-mount: Mount the filesystem in a specified directory
  :    - E.g.: ./arch.flatimage fim-mount ./mountpoint
  :- fim-xdg: Same as the 'fim-mount' command, however it opens the
  :    mount directory with xdg-open
  :- fim-perms-set: Set the permission for the container, available options are:
  :    pulseaudio, wayland, x11, session_bus, system_bus, gpu, input, usb
  :    - E.g.: ./arch.flatimage fim-perms pulseaudio,wayland,x11
  :- fim-perms-list: List the current permissions for the container
  :- fim-config-set: Sets a configuration that persists inside the image
  :    - E.g.: ./arch.flatimage fim-config-set home '"$FIM_FILE_BINARY".home
  :    - E.g.: ./arch.flatimage fim-config-set backend "proot"
  :- fim-config-list: List the current configurations for the container
  :    - E.g.: ./arch.flatimage fim-config-list                      # List all
  :    - E.g.: ./arch.flatimage fim-config-list "^dwarfs.*"          # List ones that match regex
  :- fim-dwarfs-add: Includes a dwarfs file inside the image, it is
  :                      automatically mounted on startup to the specified mount point
  :    - E.g.: ./arch.flatimage fim-dwarfs-add ./image.dwarfs /opt/image
  :- fim-dwarfs-list: Lists the dwarfs filesystems in the flatimage
  :    - E.g.: ./arch.flatimage fim-dwarfs-list
  :- fim-dwarfs-overlayfs: Makes dwarfs filesystems writteable again with overlayfs
  :    - E.g.: ./arch.flatimage fim-dwarfs-overlayfs usr '"$FIM_FILE_BINARY".config/overlays/usr'
  :- fim-help: Print this message.
	EOF
}
# }}}

# _match_free_space() {{{
# $1 Desired free space within the filesystem
# Returns the size of which the filesystem must be resized into,
# to have this maximum amount of free space when empty
function _match_free_space()
{
  declare size_new="$1"

  _msg "Target $size_new"

  # We want the to resize the free space, the current solution is to use a function
  # which is an upper bound to the growth of the free space in relation to the actual
  # filesystem size, more information is found in the spreadsheet on /doc/ext2-analysis.ods
  size_new=$(( size_new + 4*(size_new/50+2) ))

  _msg "Approx $size_new"

  echo "${size_new}"
}
# }}}

# _resize() {{{
# Changes the filesystem size
# $1 New size
function _resize()
{
  local size_new="$1"
  local size_total="$(_get_space_total "$FIM_DIR_MOUNT")"

  # When used as "+2G", to increment the current size by 2G
  if [[ "$size_new" =~ ^\+([0-9]+(K|M|G)?)$ ]]; then
    ## Adjust new size, convert to bytes
    size_new="$(numfmt --from=iec "${BASH_REMATCH[1]}")"
    ## Sum with bytes size of filesystem
    size_new="$((size_total + size_new))"
  # When used as "2G"
  elif [[ "$size_new" =~ ^([0-9]+(K|M|G)?)$ ]]; then
    size_new="$(numfmt --from=iec "${BASH_REMATCH[1]}")"
  else
    _die "Invalid size specifier '$1'"
  fi

  # Resize the maximum free space, not the filesystem size
  size_new="$(_match_free_space "$size_new")"

  # Convert to use in resize2fs
  size_new="$(numfmt --from=iec --to-unit=1Ki "$size_new")K"

  # Unmount
  _unmount

  # Resize
  "$FIM_DIR_GLOBAL_BIN"/e2fsck -fy "$FIM_FILE_BINARY"\?offset="$FIM_OFFSET" || true
  "$FIM_DIR_GLOBAL_BIN"/resize2fs "$FIM_FILE_BINARY"\?offset="$FIM_OFFSET" "${size_new}"
  "$FIM_DIR_GLOBAL_BIN"/e2fsck -fy "$FIM_FILE_BINARY"\?offset="$FIM_OFFSET" || true

  # Mount
  _mount
}
# }}}

# _resize_by_offset() {{{
function _resize_by_offset()
{
  # Get free space of current image
  local size_free="$(_get_space_free "$FIM_DIR_MOUNT")"
  # Convert to M
  size_free="$(numfmt --from=iec --to-unit=1M "$size_free")"
  # Get the minimum amount of free space allowed
  local size_slack="$(numfmt --from=iec --to-unit=1M "$FIM_SLACK_MINIMUM")"
  # Resize by minimum allowed if less than minimum allowed
  if [[ "$size_free" -lt "$size_slack" ]]; then
    _resize "+${FIM_SLACK_MINIMUM}"
  fi
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

# _get_space_total {{{
# $1 Mountpoint of a mounted filesystem
# Returns the value in bytes of the total size
function _get_space_total()
{
  local mountpoint="$1"
  if ! _is_mounted "$mountpoint"; then
    _die "No filesystem mounted in '$mountpoint'"
  fi
  { df -B1 --output=size "$mountpoint" 2>/dev/null || _die "df failed"; } | tail -n1 
}
# }}}

# _get_space_free {{{
# $1 Mountpoint of a mounted filesystem
# Returns the value in bytes of the free space
function _get_space_free()
{
  local mountpoint="$1"
  if ! _is_mounted "$mountpoint"; then
    _die "No filesystem mounted in '$mountpoint'"
  fi
  { df -B1 --output=avail "$mountpoint" 2>/dev/null || _die "df failed"; } | tail -n1 
}
# }}}

# _desktop_integration {{{
function _desktop_integration()
{
  # Name of the application as only characters delimited by _
  local name="${FIM_NAME:?"FIM_NAME is not defined"}"
  name="$(echo "$name" | tr -c '[:alnum:][=\n=]' "_")"
  name="${name,,}"

  # Icon to be integrated to the system
  local src_icon="${FIM_ICON:?"FIM_ICON is not defined"}"

  # Actual home directory
  local home="/home/$(whoami)/"

  # Icon
  if [ ! -f "$src_icon" ]; then
    _msg "Icon not found in '$src_icon'"
    return
  else
    _msg "Icon found in '$src_icon'"
  fi
  ## If type is svg, copy to scalable
  if [[ "$src_icon" =~ ^.*\.svg$ ]]; then
    _msg "Icon type is 'svg'"
    mkdir -p "$home/.local/share/icons/hicolor/scalable/mimetypes"
    mkdir -p "$home/.local/share/icons/hicolor/apps/mimetypes"
    local dest_icon_mime="$home/.local/share/icons/hicolor/scalable/mimetypes/application-flatimage_$name.svg"
    local dest_icon_app="$home/.local/share/icons/hicolor/scalable/apps/application-flatimage_$name.svg"
    if [ ! -f "$dest_icon_mime" ] || [ ! -f "$dest_icon_app" ]; then
      cp "$src_icon" "$dest_icon_mime"
      cp "$dest_icon_mime" "$dest_icon_app"
    fi
  ## Copy to varying sizes
  elif [[ "$src_icon" =~ ^.*(\.jpg|\.jpeg|\.png)$ ]]; then
    _msg "Icon type is '${src_icon##*.}'"
    for i in "16" "22" "24" "32" "48" "64" "96" "128" "256"; do
      mkdir -p "$home/.local/share/icons/hicolor/${i}x${i}/mimetypes"
      mkdir -p "$home/.local/share/icons/hicolor/${i}x${i}/apps"
      local dest_icon_mime="$home/.local/share/icons/hicolor/${i}x${i}/mimetypes/application-flatimage_$name.png"
      local dest_icon_app="$home/.local/share/icons/hicolor/${i}x${i}/apps/application-flatimage_$name.png"
      if [ ! -f "$dest_icon_mime" ] || [ ! -f "$dest_icon_app" ]; then
        magick "$src_icon" "$dest_icon_mime"
        cp "$dest_icon_mime" "$dest_icon_app"
      fi
    done
  else
    _msg "Unsupported icon format '${src_icon##*.}'"
    return
  fi
  
  # Mimetype
  local mime="$home/.local/share/mime/packages/flatimage-$name.xml"
  mkdir -p "$home/.local/share/mime/packages"
  cp "$FIM_DIR_MOUNT/fim/desktop/flatimage.xml" "$mime"
  sed -i "s|application/flatimage|application/flatimage_$name|" "$mime"
  sed -i "s|pattern=\".*flatimage\"|pattern=\"$FIM_BASENAME_BINARY\"|" "$mime"
  update-mime-database "$home/.local/share/mime"

  local categories="$(_config_fetch --value --single "categories")"

  # Desktop entry
  mkdir -p "$home/.local/share/applications"
  local entry="$home/.local/share/applications/flatimage-${name}.desktop"
  { sed -E 's/^\s+://' | tee "$entry" | sed 's/^/-- /'; } <<-END
  :[Desktop Entry]
  :Name=$name
  :Type=Application
  :Comment=FlatImage distribution of "$name"
  :TryExec=$FIM_FILE_BINARY
  :Exec="$FIM_FILE_BINARY" %F
  :Icon=application-flatimage_$name
  :MimeType=application/flatimage_$name
  :Categories=$categories
	END
}
# }}}

# _dwarfs_include {{{ 
# $1 Path to the file to include
# $2 Mountpoint
function _dwarfs_include()
{
  # Default directory for dwarfs files
  mkdir -p "$FIM_DIR_DWARFS"

  # Input file
  local path_file_host="$(readlink -f "$1")"
  FIM_DEBUG=1 _msg "Input file: $path_file_host"

  # Basename, only leave stem
  local basename_file_host="$(basename -s .dwarfs "$path_file_host")"
  # # Normalize
  basename_file_host="$(echo "$basename_file_host" | tr -d -c '[:alnum:][:space:][=.=][=-=]' | xargs)"
  basename_file_host="$(echo "$basename_file_host" | tr ' ' '-')"

  # Save as
  local path_file_guest="$FIM_DIR_DWARFS/$basename_file_host.dwarfs"
  FIM_DEBUG=1 _msg "Save as: $path_file_guest"

  # Target
  if [[ -z "$2" ]]; then
    _die "Mountpoint must not be empty"
  fi
  local path_rel_mountpoint="/$2"
  local path_abs_mountpoint="$FIM_DIR_MOUNT/$2"

  # Verify if the input file exists and is a regular file
  if [[ ! -f "$path_file_host" ]]; then
    _die "File '$path_file_host' does not exist or is not a regular file"
  fi
  if df --output=target "$path_file_host" 2>/dev/null | grep -i "$FIM_DIR_MOUNT" &>/dev/null; then
    _die "Target cannot not be inside the guest filesystem"
  fi

  # Check if the dwarfs file is already in the container
  if [[ -f "$path_file_guest" ]]; then
    FIM_DEBUG=1 _msg "File '$path_file_guest' already exists overwriting..."
  elif [[ -e "$path_file_guest" ]]; then
    _die "File '$path_file_guest' already exists and is not a regular file"
  fi

  # Check if mountpoint exists and is not a symlink
  if [[ -e "$path_abs_mountpoint" ]] && [[ ! -h "$path_abs_mountpoint" ]]; then
    _die "Mountpoint '$path_abs_mountpoint' exists and is not symbolic link"
  fi

  # Replace if exists
  rm -fv "$path_file_guest"

  # Get size of target to include
  local size_target="$(du -sb "$path_file_host" | awk '{print $1}')"
  [[ "$size_target" =~ ^[0-9]+$ ]] || _die "size_target is NaN: '$size_target'"
  FIM_DEBUG=1 _msg "Size of file to include is of '$(numfmt --from=iec --to-unit=1M "${size_target}")M'"

  # Get current free space
  local size_free="$(_get_space_free "$FIM_DIR_MOUNT")"

  # Resize by the amount required to fit
  _resize "+${size_target}"

  # Include dwarfs inside container
  FIM_DEBUG=1 _msg "Include target '$path_file_host' in '$path_file_guest'"
  cp -f "$path_file_host" "$path_file_guest"

  # Update dwarfs filesystem table
  _config_set "dwarfs.$basename_file_host" "$path_rel_mountpoint"
}
# }}}

# _dwarfs_list {{{ 
function _dwarfs_list()
{
  for i in "$FIM_DIR_DWARFS"/*; do
    basename -s .dwarfs "$i"
  done
}
# }}}

# _dwarfs_overlay {{{ 
# $1 Dwarfs filesystem to overlay
# $2 Mountpoint
function _dwarfs_overlay()
{
  # Create full path to filesystem
  local path_file_dwarfs="$FIM_DIR_DWARFS/$1.dwarfs"

  # Check if exists
  if [[ ! -f "$path_file_dwarfs" ]]; then
    _die "Filesystem '$path_file_dwarfs' not found for overlayfs setup"
  fi

  # Check if mountpoint is not empty string
  local mountpoint="$2"
  if [[ -z "$mountpoint" ]]; then
    _die "Empty mountpoint argument"
  fi

  # Create overlayfs configuration
  local basename_file_dwarfs="$(basename -s .dwarfs "$path_file_dwarfs")"
  _config_set "dwarfs.overlay.$basename_file_dwarfs" "$mountpoint"
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
  rm "$FIM_FILE_BINARY"

  # Copy startup binary
  cp "$FIM_DIR_TEMP/main" "$FIM_FILE_BINARY"

  # Append tools
  cat "$FIM_DIR_GLOBAL_BIN"/{fuse2fs,e2fsck,bash}  >> "$FIM_FILE_BINARY"

  # Update offset
  FIM_OFFSET="$(du -sb "$FIM_FILE_BINARY" | awk '{print $1}')"

  # Adjust free space
  size_image="$(_match_free_space "$size_image")"

  # Create filesystem
  truncate -s "$size_image" "$file_image"

  # Check block size of host
  local block_size="$(stat -fc %s .)"
  _msg "block size: $block_size"

  # Format as ext2
  "$FIM_DIR_GLOBAL_BIN"/mke2fs -F -b"$(stat -fc %s .)" -t ext2 "$file_image" &> "$FIM_STREAM"

  # Re-format and include files
  "$FIM_DIR_GLOBAL_BIN"/mke2fs -F -d "$dir_system" -b"$(stat -fc %s .)" -t ext2 "$file_image" &> "$FIM_STREAM"

  # Append filesystem to binary
  cat "$file_image" >> "$FIM_FILE_BINARY"

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
    local mountpoint="${FIM_DIR_MOUNT}.mount.dwarfs.$(basename -s .dwarfs "$i")"
    # Log
    _msg "DWARFS FS: $filesystem_file"
    _msg "DWARFS MP: $mountpoint"
    # Save
    FIM_MOUNTS_DWARFS["$filesystem_file"]="$mountpoint"
  done < <(find "$FIM_DIR_DWARFS" -maxdepth 1 -iname "*.dwarfs")
}
# }}}

# _find_overlayfs() {{{
# Finds the mountpoints for the overlayfs filesystems
# Populates FIM_MOUNTS_OVERLAYFS (src dir -> target dir)
function _find_overlayfs()
{
  while read -r overlay; do
    # Filesystem
    local basename_filesystem="${overlay##dwarfs.overlay.}"
    # Filesystem path
    local path_dwarfs_filesystem="$FIM_DIR_DWARFS/${basename_filesystem}.dwarfs"
    # Check if exists
    if [[ ! -f "$path_dwarfs_filesystem" ]]; then
      FIM_DEBUG=1 _msg "Dwarfs filesystem not found in '$path_dwarfs_filesystem'"
      return
    fi
    _msg "OVERLAYFS FS: $path_dwarfs_filesystem"
    # Fetch paths
    # # This is the dwarfs mount point
    local bind_cont="$(_config_fetch --single --value "dwarfs.$basename_filesystem")"
    # # This is the overlayfs mount point, it will overlay the dwarfs mount point
    local bind_host="$(_config_fetch --single --value "$overlay")"
    # Check empty string
    [[ -n "$bind_cont" ]] || { FIM_DEBUG=1 _msg "Could not find dwarfs filesystem 'dwarfs.$basename_filesystem'"; break; }
    [[ -n "$bind_host" ]] || { FIM_DEBUG=1 _msg "Could not find mount point for overlay '$overlay'"; break; }
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
  done < <(_config_fetch --key "dwarfs.overlay")
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
    # Get path to symlink to
    local symlink_target="$(_config_fetch --single --value "dwarfs.$(basename -s .dwarfs "$i")")"
    _msg "DWARFS SYMLINK TARGET: $symlink_target"
    symlink_target="$FIM_DIR_MOUNT/$symlink_target"
    mkdir -p "$(dirname "$symlink_target")"
    _msg "DWARFS SYMLINK PATH: $symlink_target"
    # Symlink, skip if directory exists
    ln -T -sfnv "$mp" "$symlink_target" &>"$FIM_STREAM" || continue
    # Mount
    "$FIM_DIR_GLOBAL_BIN/dwarfs" "$fs" "$mp" &> "$FIM_STREAM" || continue
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
    _msg "OVERLAYFS workdir: $workdir"
    _msg "OVERLAYFS upperdir: $upperdir"
    # Create host-sided paths
    mkdir -pv "$bind_host"/{workdir,upperdir,mount} &> "$FIM_STREAM"
    # If is a symlink from inside the container that points to a directory in the host
    # # Update bind_cont with resolved path (might be a symlink created by dwarfs)
    # # Replace symlink to point to mount to overlayfs
    local bind_symlink="$bind_cont"
    if test -h "$bind_cont"; then
      _msg "OVERLAYFS unresolved: $bind_symlink"
      bind_cont="$(readlink -f "$bind_cont")"
      _msg "OVERLAYFS lowerdir: $bind_cont"
    else
      FIM_DEBUG=1 _msg "Overlay container mount '$bind_cont' not a symlink"
      break
    fi
    # Define mount point for overlayfs
    local basename_mount="$(basename "$bind_cont")"
    basename_mount="${basename_mount##*dwarfs.}"
    _msg "basename mount: $basename_mount"
    local mount="${FIM_DIR_MOUNT}.mount.overlayfs.$basename_mount"
    mkdir -vp "$mount" &> "$FIM_STREAM"
    _msg "OVERLAYFS mount: $mount"
    ln -T -sfn "$mount" "$bind_symlink"
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
  # Find mountpoints
  _find_dwarfs
  _find_overlayfs

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

  # Set env for the container
  # shellcheck disable=2016
  _cmd_bwrap+=('env PATH="$FIM_DIR_STATIC:$PATH"')

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

  # Load pre hooks, allow to fail
  set +e
  for hook in "$FIM_DIR_HOOKS"/pre/*; do
    #shellcheck disable=1090
    . "$hook"
  done
  set -e

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

  # Load post hooks, allow to fail
  set +e
  for hook in "$FIM_DIR_HOOKS"/post/*; do
    #shellcheck disable=1090
    . "$hook"
  done
  set -e
}
# }}}

# _compress() {{{
# Subdirectory compression
function _compress()
{
  [ -n "$FIM_RW" ] || _die "Set FIM_RW to 1 before compression"

  # Remove apt lists and cache
  rm -rf "$FIM_DIR_MOUNT"/var/{lib/apt/lists,cache}

  # Create temporary directory to fit-resize fs
  local dir_rebuild="$FIM_DIR_BINARY/$FIM_BASENAME_BINARY.tmp"
  rm -rf "$dir_rebuild"
  mkdir -p "$dir_rebuild"

  # Copy flatimage dir
  cp -r "$FIM_DIR_MOUNT/fim" "$dir_rebuild"

  # Create dwarfs dir if not already exists
  mkdir -p "$dir_rebuild/fim/dwarfs"

  # Compress selected directories
  for i in ${FIM_COMPRESSION_DIRS}; do
    local basename_target="$(basename "$i")"
    local path_dir_target="$FIM_DIR_MOUNT/$i"
    # Check if folder exists inside container
    [ -d "$path_dir_target" ] || _die "Folder $path_dir_target not found for compression"
    # Compress folder
    "$FIM_DIR_GLOBAL_BIN/mkdwarfs" \
      -f \
      -i "$path_dir_target" \
      -o "$dir_rebuild/fim/dwarfs/$basename_target.dwarfs" \
      -l "$FIM_COMPRESSION_LEVEL"
    # Set mountpoint as the basename
    _config_set "dwarfs.${basename_target}" "/$i"
    # Remove it from inside the container
    rm -rf "$path_dir_target"
  done

  # Copy config file with update dwarfs mount points
  cp "$FIM_FILE_CONFIG" "$dir_rebuild/fim/$(basename "$FIM_FILE_CONFIG")"

  # Remove remaining files from dev to avoid binding issues
  rm -rf "${FIM_DIR_MOUNT:?"Empty FIM_DIR_MOUNT"}"/dev

  # Move files to temporary directory
  for i in "$FIM_DIR_MOUNT"/{bin,etc,lib,lib64,opt,root,run,sbin,share,tmp,usr,var}; do
    { mv "$i" "$dir_rebuild" || true; } &>"$FIM_STREAM"
  done

  # Update permissions
  chmod -R +rw "$dir_rebuild"

  # Resize to fit files size + slack
  local size_files="$(du -sb "$dir_rebuild" | awk '{print $1}')"
  local size_slack="$FIM_SLACK_MINIMUM";
  local size_new="$((size_files+size_slack))"

  _msg "Size files        : $size_files"
  _msg "Size slack        : $size_slack"
  _msg "Size sum          : $size_new"

  # Resize
  _rebuild "$size_new" "$dir_rebuild"

  # Remove mount dirs
  rm -rf "${FIM_DIR_MOUNT:?"Empty mount var"}"/{tmp,proc,sys,dev,run}

  # Remove temporary rebuild dir
  rm -rf "$dir_rebuild"

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
    if [[ "$i" =~ ^([^=]*)=(.*) ]]; then
      # Get Entry
      local match_key="$(echo "${BASH_REMATCH[1]}" | xargs)"
      local match_value="$(echo "${BASH_REMATCH[2]}" | xargs)"
      # Check match
      if ! [[ "$match_key" =~ $regex ]]; then continue; fi
      # Return match
      if [[ "$value" -eq 1 ]]; then
        echo "$match_value" | xargs
      elif [[ "$key" -eq 1 ]]; then
        echo "$match_key" | xargs
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
  local key="$1"; shift
  local val="$*"

  # Sanitize
  key="$(echo "$key" | tr -d -c '[:alnum:][=-=][=_=][=.=][=$=][="=][=/=]' | xargs)"
  val="$(echo "$val" | tr -d -c '[:alnum:][:space:][=-=][=_=][=.=][=$=][="=][=/=]' | xargs)"

  local entry="$key = $val"

  if grep "$key =" "$FIM_FILE_CONFIG" &>"$FIM_STREAM"; then
    sed -i "s|$key =.*|$entry|" "$FIM_FILE_CONFIG"
  else
    echo "$entry" >> "$FIM_FILE_CONFIG"
  fi
}
# }}}

# _main() {{{
function _main()
{
  _msg "FIM_OFFSET           : $FIM_OFFSET"
  _msg "FIM_RO               : $FIM_RO"
  _msg "FIM_RW               : $FIM_RW"
  _msg "FIM_STREAM           : $FIM_STREAM"
  _msg "FIM_SLACK_MINIMUM    : $FIM_SLACK_MINIMUM"
  _msg "FIM_ROOT             : $FIM_ROOT"
  _msg "FIM_NORM             : $FIM_NORM"
  _msg "FIM_DEBUG            : $FIM_DEBUG"
  _msg "FIM_NDEBUG           : $FIM_NDEBUG"
  _msg "FIM_DIR_GLOBAL       : $FIM_DIR_GLOBAL"
  _msg "FIM_DIR_GLOBAL_BIN   : $FIM_DIR_GLOBAL_BIN"
  _msg "FIM_DIR_MOUNT        : $FIM_DIR_MOUNT"
  _msg "FIM_DIR_TEMP         : $FIM_DIR_TEMP"
  _msg "FIM_FILE_BINARY      : $FIM_FILE_BINARY"
  _msg "FIM_BASENAME_BINARY  : $FIM_BASENAME_BINARY"
  _msg "FIM_DIR_BINARY       : $FIM_DIR_BINARY"
  _msg '$*                   : '"$*"

  # Check filesystem
  "$FIM_DIR_GLOBAL_BIN"/e2fsck -fy "$FIM_FILE_BINARY"\?offset="$FIM_OFFSET" &> "$FIM_STREAM" || true

  # Copy tools
  declare -a ext_tools=(
    "[" "b2sum" "base32" "base64" "basename" "cat" "chcon" "chgrp" "chmod" "chown" "chroot" "cksum"
    "comm" "cp" "csplit" "cut" "date" "dcgen" "dd" "df" "dir" "dircolors" "dirname" "du" "dwarfs"
    "echo" "env" "expand" "expr" "factor" "false" "fmt" "fold" "groups" "head" "hostid" "id" "join"
    "kill" "link" "ln" "logname" "ls" "magick" "md5sum" "mkdir" "mkdwarfs" "mkfifo" "mknod" "mktemp"
    "mv" "nice" "nl" "nohup" "nproc" "numfmt" "od" "overlayfs" "paste" "pathchk" "pr" "printenv"
    "printf" "ptx" "pwd" "readlink" "realpath" "rm" "rmdir" "runcon" "seq" "sha1sum" "sha224sum"
    "sha256sum" "sha384sum" "sha512sum" "shred" "shuf" "sleep" "sort" "split" "stat" "stty" "sum"
    "sync" "tac" "tail" "tee" "test" "timeout" "touch" "tr" "true" "truncate" "tsort" "uname"
    "unexpand" "uniq" "unlink" "uptime" "users" "vdir" "wc" "who" "whoami" "yes" "resize2fs"
    "mke2fs" "lsof"
  )

  # Setup daemon to unmount filesystems on exit
  chmod +x "$FIM_FPATH_KILLER"
  nohup "$FIM_FPATH_KILLER" &>/dev/null & disown

  # Copy static binaries
  _copy_tools "${ext_tools[@]}"

  # Mount filesystem
  _mount

  # Resize by offset if space is less than minimum
  _resize_by_offset

  # Check if config exists, else try to touch if mounted as RW
  [ -f "$FIM_FILE_CONFIG" ] || { [ -n "$FIM_RO" ] || touch "$FIM_FILE_CONFIG"; }

  # Check if custom home directory is set
  local home="$(_config_fetch --value --single "home")"
  # # Expand
  home="$(eval echo "$home")"
  # # Set & show on debug mode
  [[ -z "$home" ]] || { mkdir -p "$home" && export HOME="$home"; }
  _msg "FIM_HOME        : $HOME"

  # Check if custom package name is set
  export FIM_NAME="$(_config_fetch --value --single "name")"
  # # Expand
  FIM_NAME="$(eval echo "$FIM_NAME")"
  # # Set default
  FIM_NAME="${FIM_NAME:-"${FIM_DIST,,}"}"
  _msg "FIM_NAME        : $FIM_NAME"

  # Check for custom icon
  export FIM_ICON="$(_config_fetch --value --single "icon")"
  # # Expand
  FIM_ICON="$(eval echo "$FIM_ICON")"
  # # Set default
  if [ ! -f "$FIM_ICON" ]; then
    FIM_ICON="$FIM_DIR_MOUNT/fim/desktop/icon.svg"
  fi
  _msg "FIM_ICON        : $FIM_ICON"

  # Setup desktop integration
  local fim_desktop="$(_config_fetch --value --single "desktop")"
  if [ "$fim_desktop" = "1" ]; then
    set +e; _desktop_integration &>"$FIM_STREAM"; set -e
  fi

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
      "xdg") _re_mount "$2"; xdg-open "$2"; read -r ;;
      "mount") _re_mount "$2"; read -r ;;
      "config-list") shift; _config_fetch "$*" ;;
      "config-set") _config_set "$2" "$3";;
      "perms-list") _perms_list ;;
      "perms-set") _perms_set "$2";;
      "dwarfs-add") _dwarfs_include "$2" "$3" ;;
      "dwarfs-list") _dwarfs_list ;;
      "dwarfs-overlayfs") _dwarfs_overlay "$2" "$3" ;;
      "help") _help;;
      *) _help; _die "Unknown fim command" ;;
    esac
  else
    local default_cmd="$(_config_fetch --value --single "cmd")"
    _exec  "${default_cmd:-"$FIM_FILE_BASH"}" "$@"
  fi

}
# }}}

_main "$@"

#  vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :
