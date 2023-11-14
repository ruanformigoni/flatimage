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

PID="$$"

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
export FIM_PATH_FILE_BINARY="${FIM_PATH_FILE_BINARY:?FIM_PATH_FILE_BINARY is unset or null}"
export FIM_FILE_BINARY="$(basename "$FIM_PATH_FILE_BINARY")"
export FIM_DIR_BINARY="$(dirname "$FIM_PATH_FILE_BINARY")"
export FIM_FILE_BASH="$FIM_DIR_GLOBAL_BIN/bash"
export BASHRC_FILE="$FIM_DIR_TEMP/.bashrc"
export FIM_FILE_PERMS="$FIM_DIR_MOUNT"/fim/perms

# Give static tools priority in PATH
export PATH="$FIM_DIR_STATIC:$PATH"

# Compression
export FIM_COMPRESSION_LEVEL="${FIM_COMPRESSION_LEVEL:-4}"
export FIM_COMPRESSION_SLACK="${FIM_COMPRESSION_SLACK:-50000000}" # 50MB
export FIM_COMPRESSION_DIRS="${FIM_COMPRESSION_DIRS:-/usr /opt}"

# Output stream
export FIM_STREAM="${FIM_DEBUG:+/dev/stdout}"
export FIM_STREAM="${FIM_STREAM:-/dev/null}"

# shopt
shopt -s nullglob

# _msg() {{{
# Emits a message in &2
# $(1..n-1) arguments to echo
# $n message
function _msg()
{
  [ -z "$FIM_DEBUG" ] || echo -e "${@:1:${#@}-1}" "[\033[32m*\033[m] ${*: -1}" >&2;
}
# }}}

# _wait() {{{
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
  # Get parent pid
  local ppid="$(pgrep -f "fuse2fs.*offset=$FIM_OFFSET.*$FIM_PATH_FILE_BINARY")"

  fusermount -zu "$FIM_DIR_MOUNT"

  _wait "$ppid"
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
  [ -z "$*" ] || FIM_DEBUG=1 _msg "$*"
  # Unmount dwarfs
  local sha="$(_config_fetch --value --single "sha")"
  if [ -n "$sha" ]; then
    shopt -s nullglob
    for i in "$FIM_DIR_GLOBAL"/dwarfs/"$sha"/*; do
      # Check if is mounted
      if mount | grep "$i" &>/dev/null; then
        # Get parent pid
        local ppid="$(pgrep -f "dwarfs2.*$i")"
        fusermount -zu "$FIM_DIR_GLOBAL"/dwarfs/"$sha"/"$(basename "$i")" &> "$FIM_STREAM" || true
        _wait "$ppid"
      fi
    done
  fi
  # Unmount image
  _unmount &> "$FIM_STREAM"
  # Exit
  kill -s SIGTERM "$PID"
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

# _match_free_space() {{{
# The size of a filesystem and the free size of a filesystem differ
# This makes the free space on the filesystem match the target size
# $1 filesystem file
# $2 target size
function _match_free_space()
{
  # Get filesystem file
  local file_filesystem="$1"
  local mount="$FIM_DIR_BINARY/${FIM_FILE_BINARY}.mount"

  mkdir -p "$mount"

  # Get target size
  declare -i target="$2"
  target="$((target / 1024))"
  [[ "$target" =~ ^[0-9]+$ ]] || _die "target is NaN"

  while :; do
    # Get current free size
    "$FIM_DIR_GLOBAL_BIN"/fuse2fs "$file_filesystem" "$mount"
    ## Wait for mount
    sleep 1
    ## Grep free size
    declare -i curr_free="$(df -B1 -P | grep -i "$mount" | awk '{print $4}')"
    ## Wait for mount process termination
    local pid_fuse2fs="$(pgrep -f "fuse2fs.*$file_filesystem")"
    fusermount -u "$mount"
    _wait "$pid_fuse2fs"
    ## Check if got an integral number
    [[ "$curr_free" =~ ^[0-9]+$ ]] || _die "curr_free is NaN"
    ## Convert from bytes to kibibytes
    curr_free="$((curr_free / 1024))"
    _msg "Free space $curr_free"

    # Check if has reached desired size
    if [[ "$curr_free" -gt "$target" ]]; then break; fi

    # Get the curr total
    declare -i curr_total="$(du -sb "$file_filesystem" | awk '{print $1}')"
    [[ "$curr_total" =~ ^[0-9]+$ ]] || _die "curr_total is NaN"
    curr_total="$((curr_total / 1024))"

    # Increase on step
    declare -i step="50000"

    # Calculate new size
    declare -i new_size="$((curr_total+step))"

    # Include size limit
    if [[ "$new_size" -gt "10000000" ]];  then _die "Too large filesystem resize attempt"; fi

    # Resize
    "$FIM_DIR_GLOBAL_BIN"/e2fsck -fy "$file_filesystem" &> "$FIM_STREAM" || true
    "$FIM_DIR_GLOBAL_BIN"/resize2fs "$file_filesystem" "${new_size}K" &> "$FIM_STREAM"
    "$FIM_DIR_GLOBAL_BIN"/e2fsck -fy "$file_filesystem" &> "$FIM_STREAM" || true

    _msg "Target of $target"
    _msg "Resize from $curr_total to $new_size"
  done

  rmdir "$mount"
}
# }}}

# _rebuild() {{{
# Re-create the filesystem with new data
# $1 New size
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
  _match_free_space "$file_image" "$size_image"

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

# _exec() {{{
# Chroots into the filesystem
# $* Command and args
function _exec()
{
  # Check for empty string
  [ -n "$*" ] || FIM_DEBUG=1 _msg "Empty arguments for exec"

  # Fetch CMD
  declare -a cmd
  for i; do
    [ -z "$i" ] || cmd+=("\"$i\"")
  done

  _msg "cmd: ${cmd[*]}"

  # Fetch SHA
  export DWARFS_SHA="$(_config_fetch --value --single "sha")"
  _msg "DWARFS_SHA: $DWARFS_SHA"

  # Mount dwarfs files if exist
  [ -f "$FIM_DIR_GLOBAL_BIN/dwarfs" ]  || cp "$FIM_DIR_MOUNT/fim/static/dwarfs" "$FIM_DIR_GLOBAL_BIN"/dwarfs
  chmod +x "$FIM_DIR_GLOBAL_BIN/dwarfs"

  # shellcheck disable=2044
  for i in $(find "$FIM_DIR_MOUNT" -maxdepth 1 -iname "*.dwarfs"); do
    i="$(basename "$i")"
    local fs="$FIM_DIR_MOUNT/$i"
    local mp="$FIM_DIR_GLOBAL/dwarfs/$DWARFS_SHA/${i%.dwarfs}"; mkdir -p "$mp"
    "$FIM_DIR_GLOBAL_BIN/dwarfs" "$fs" "$mp" &> "$FIM_STREAM"
  done

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
      _cmd_bwrap+=("--bind \"$i\" \"$i\"")
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
      nvidia_binds+=(/usr/bin/*nvidia*)
      nvidia_binds+=(/usr/share/*nvidia*)
      nvidia_binds+=(/usr/share/vulkan/icd.d/*nvidia*)
      nvidia_binds+=(/usr/lib32/*nvidia*)
      nvidia_binds+=(/usr/lib32/*cuda*)
      nvidia_binds+=(/usr/lib32/*vdpau*)
      for i in "${nvidia_binds[@]}"; do
        _cmd_bwrap+=("--bind \"$i\" \"$i\"")
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
  local size_slack="$FIM_COMPRESSION_SLACK";
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

  # Parse args
  while :; do
    case "$1" in
      --single) single=1; shift ;;
      --value) value=1; shift ;;
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
        [[ "$i" =~ ([^=]*)=(.*) ]] && echo "${BASH_REMATCH[2]}"
      else
        echo "$i"
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

  if grep "$opt" "$FIM_FILE_CONFIG" &>"$FIM_STREAM"; then
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
  _copy_tools "resize2fs" "mke2fs"

  # Mount filesystem
  _mount

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
      "xdg") _re_mount "$2"; xdg-open "$2"; read -r ;;
      "mount") _re_mount "$2"; read -r ;;
      "config-list") shift; _config_fetch "$*" ;;
      "config-set") _config_set "$2" "$3";;
      "perms-list") _perms_list ;;
      "perms-set") _perms_set "$2";;
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
