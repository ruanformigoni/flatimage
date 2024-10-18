#!/bin/sh

set -e

_die()
{
  echo "$*" >&2
  kill -s TERM "$$"
}

_requires()
{
  for i; do
    command -v "$i" 1>/dev/null 2>/dev/null || _die "Dependency '$i' not found, please install it separately"
  done
}

_main()
{
  _requires "wget"

  mkdir -p build && cd build

  # Use jq to fetch latest flatimage version from github
  mkdir -p bin
  [ -f "bin/jq" ] || wget -O bin/jq "https://github.com/jqlang/jq/releases/download/jq-1.7/jq-linux-amd64"
  [ -f "bin/xz" ] || wget -O bin/xz "https://github.com/ruanformigoni/xz-static-musl/releases/download/fec8a15/xz"
  [ -f "bin/busybox" ] || wget -O bin/busybox "https://github.com/ruanformigoni/busybox-static-musl/releases/download/7e2c5b6/busybox-x86_64"
  ln -sf busybox bin/tar
  chmod +x ./bin/*
  export PATH="$(pwd)/bin:$PATH"

  # Download flatimage
  IMAGE="./bin/arch.flatimage"
  [ -f "$IMAGE" ] || wget -O "$IMAGE" "https://github.com/ruanformigoni/flatimage/releases/download/v1.0.2/arch.flatimage"
  chmod +x "$IMAGE"

  # Enable network
  "$IMAGE" fim-perms set network

  # Update image
  "$IMAGE" fim-root pacman -Syu --noconfirm

  # Install dependencies
  ## General
  "$IMAGE" fim-root pacman -S --noconfirm xorg-server mesa lib32-mesa glxinfo lib32-gcc-libs \
    gcc-libs pcre freetype2 lib32-freetype2
  ## Video AMD/Intel
  "$IMAGE" fim-root pacman -S --noconfirm xf86-video-amdgpu vulkan-radeon lib32-vulkan-radeon vulkan-tools
  "$IMAGE" fim-root pacman -S --noconfirm xf86-video-intel vulkan-intel lib32-vulkan-intel vulkan-tools

  # Install steam
  ## Select the appropriated drivers for your GPU when asked
  "$IMAGE" fim-root pacman -S --noconfirm steam

  # Clear cache
  "$IMAGE" fim-root pacman -Scc --noconfirm

  # Set permissions
  "$IMAGE" fim-perms set media,audio,wayland,xorg,udev,dbus_user,usb,input,gpu,network

  # Configure user name and home directory
  "$IMAGE" fim-exec mkdir -p /home/steam
  "$IMAGE" fim-env add 'USER=steam' \
    'HOME=/home/steam' \
    'XDG_CONFIG_HOME=/home/steam/.config' \
    'XDG_DATA_HOME=/home/steam/.local/share'

  # Set command to run by default
  "$IMAGE" fim-boot /usr/bin/steam

  # Configure desktop integration
  wget -O steam.svg https://upload.wikimedia.org/wikipedia/commons/8/83/Steam_icon_logo.svg
  tee steam.json <<-EOF
  {
    "name": "Steam",
    "icon": "./steam.svg",
    "categories": ["Game"]
  }
EOF
  "$IMAGE" fim-desktop setup ./steam.json
  "$IMAGE" fim-desktop enable icon,mimetype,entry

  # Notify the application has started
  "$IMAGE" fim-notify on

  # Compress
  "$IMAGE" fim-commit

  # Copy
  rm -rf ../steam.flatimage
  mv "$IMAGE" ../steam.flatimage
}

_main "$@"
