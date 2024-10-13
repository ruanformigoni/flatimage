#!/bin/bash

set -e

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
  wget -q --show-progress --progress=dot:binary -O bin/jq "https://github.com/jqlang/jq/releases/download/jq-1.7/jq-linux-amd64"
  chmod +x ./bin/*

  # Download flatimage
  local image="./bin/arch.flatimage"
  wget -q --show-progress --progress=dot:mega -O "$image" "https://github.com/ruanformigoni/flatimage/releases/download/v1.0.0/arch.flatimage"
  chmod +x "$image"

  # Enable network
  "$image" fim-perms set network

  # Configure PATH variable
  "$image" fim-env add 'PATH="/usr/bin:$PATH"'

  # Update image
  "$image" fim-root pacman -Syu --noconfirm

  # Install dependencies
  ## General
  "$image" fim-root pacman -S --noconfirm xorg-server mesa lib32-mesa glxinfo lib32-gcc-libs \
    gcc-libs pcre freetype2 lib32-freetype2
  ## Video AMD/Intel
  "$image" fim-root pacman -S --noconfirm xf86-video-amdgpu vulkan-radeon lib32-vulkan-radeon vulkan-tools
  "$image" fim-root pacman -S --noconfirm xf86-video-intel vulkan-intel lib32-vulkan-intel vulkan-tools

  # Install steam
  ## Select the appropriated drivers for your GPU when asked
  "$image" fim-root pacman -S --noconfirm steam

  # Clear cache
  "$image" fim-root pacman -Scc --noconfirm

  # Set permissions
  "$image" fim-perms set media,audio,wayland,xorg,udev,dbus_user,usb,input,gpu,network

  # Configure user name and home directory
  "$image" fim-exec mkdir -p /home/steam
  "$image" fim-env add 'USER=steam' \
    'HOME=/home/steam' \
    'XDG_CONFIG_HOME=/home/steam/.config' \
    'XDG_DATA_HOME=/home/steam/.local/share'

  # Set command to run by default
  "$image" fim-boot /usr/bin/steam

  # Compress
  "$image" fim-commit

  # Copy
  cp "$image" ../steam.flatimage
}

_main "$@"
