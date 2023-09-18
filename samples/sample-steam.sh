#!/bin/bash

set -e

export PID="$$"

function _die()
{
  echo "$*" >&2
  kill -s TERM "$$"
}

function _requires()
{
  for i; do
    command -v "$i" &>/dev/null || _die "Dependency '$i' not found, please install it separately"
  done
}

function _main()
{
  _requires "tar" "wget" "xz"

  # Fetch arch flatimage
  tarball_arch="arch.tar.xz"
  if ! [[ -f "$tarball_arch" ]]; then
    wget --progress=dot:giga "https://gitlab.com/api/v4/projects/43000137/packages/generic/fim/continuous/$tarball_arch"
  fi

  # Extract
  if command -v pv &>/dev/null; then
    pv -per "$tarball_arch" | tar -Jxf -
  else
    echo "Command 'pv' not found, install it to monitor the progress of extracting the archive"
    tar -xf "$tarball_arch"
  fi

  # Resize
  ./arch.fim fim-resize 4G

  # Set default home directory
  ./arch.fim fim-config-set home '"$FIM_DIR_BINARY"/home.steam'

  # Install steam
  ## Select the appropriated drivers for your GPU when asked
  ./arch.fim fim-root fakechroot pacman -S steam

  # Set permissions
  ./arch.fim fim-perms-set wayland,x11,gpu,session_bus,pulseaudio,input

  # Set command to run by default
  ./arch.fim fim-cmd steam

  # Clear cache
  ./arch.fim fim-root fakechroot pacman -Scc --noconfirm

  # Compress
  ./arch.fim fim-compress

  # Rename
  mv ./arch.fim ./steam
}

_main "$@"

#  vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :
