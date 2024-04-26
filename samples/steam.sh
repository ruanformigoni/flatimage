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

  mkdir -p build && cd build

  # Use jq to fetch latest flatimage version from github
  mkdir -p bin
  export PATH="$(pwd)/bin:$PATH"
  wget -q --show-progress --progress=dot:binary -O bin/jq https://github.com/jqlang/jq/releases/download/jq-1.7/jq-linux-amd64
  chmod +x ./bin/*

  # Fetch latest flatimage version from github
  local tarball_arch="arch.tar.xz"
  if [[ ! -f "$tarball_arch" ]]; then
    wget "$(wget -qO - "https://api.github.com/repos/ruanformigoni/flatimage/releases/latest" \
      | jq -r '.assets.[].browser_download_url | match(".*arch.tar.xz$").string')"
  fi

  # Extract
  if command -v pv &>/dev/null; then
    pv -per "$tarball_arch" | tar -Jxf -
  else
    echo "Command 'pv' not found, install it to monitor the progress of extracting the archive"
    tar -xf "$tarball_arch"
  fi

  local image=$(pwd)/arch.flatimage

  # Resize
  "$image" fim-resize 4G

  # Set default home directory
  "$image" fim-config-set home '"$FIM_DIR_BINARY"/steam.home'

  # Update image
  "$image" fim-root fakechroot pacman -Syu --noconfirm

  # Install dependencies
  ## General
  "$image" fim-root fakechroot pacman -S --noconfirm xorg-server mesa lib32-mesa glxinfo lib32-gcc-libs \
    gcc-libs pcre freetype2 lib32-freetype2
  ## Video AMD/Intel
  "$image" fim-root fakechroot pacman -S --noconfirm xf86-video-amdgpu vulkan-radeon lib32-vulkan-radeon vulkan-tools
  "$image" fim-root fakechroot pacman -S --noconfirm xf86-video-intel vulkan-intel lib32-vulkan-intel vulkan-tools

  # Install steam
  ## Select the appropriated drivers for your GPU when asked
  "$image" fim-root fakechroot pacman -S --noconfirm steam

  # Set permissions
  "$image" fim-perms-set wayland,x11,gpu,session_bus,pulseaudio,input

  # Set command to run by default
  "$image" fim-cmd steam

  # Clear cache
  "$image" fim-root fakechroot pacman -Scc --noconfirm

  # Compress
  "$image" fim-compress

  # Rename
  mv "$image" ../steam
}

_main "$@"

#  vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :
