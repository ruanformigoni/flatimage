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
  IMAGE="./bin/alpine.flatimage"
  [ -f "$IMAGE" ] || wget -O "$IMAGE" "$(curl -H "Accept: application/vnd.github+json" \
    https://api.github.com/repos/ruanformigoni/flatimage/releases/latest 2>/dev/null \
    | jq -e -r '.assets.[].browser_download_url | match(".*alpine.flatimage$").string')"
  chmod +x "$IMAGE"

  # Enable network
  "$IMAGE" fim-perms set network

  # Update image
  "$IMAGE" fim-root apk update --no-cache

  # Install dependencies
  "$IMAGE" fim-root apk add --no-cache font-noto

  # Install firefox
  "$IMAGE" fim-root apk add --no-cache firefox

  # Set permissions
  "$IMAGE" fim-perms set media,audio,wayland,xorg,udev,dbus_user,usb,input,gpu,network

  # Configure user name and home directory
  "$IMAGE" fim-exec mkdir -p /home/firefox
  "$IMAGE" fim-env add 'USER=firefox' \
    'HOME=/home/firefox' \
    'XDG_CONFIG_HOME=/home/firefox/.config' \
    'XDG_DATA_HOME=/home/firefox/.local/share'

  # Set command to run by default
  "$IMAGE" fim-boot /usr/bin/firefox

  # Configure desktop integration
  wget -O firefox.svg "https://upload.wikimedia.org/wikipedia/commons/a/a0/Firefox_logo%2C_2019.svg"
  tee desktop.json <<-EOF
  {
    "name": "firefox",
    "icon": "./firefox.svg",
    "categories": ["Network"]
  }
EOF
  "$IMAGE" fim-desktop setup ./desktop.json
  "$IMAGE" fim-desktop enable icon,mimetype,entry

  # Notify the application has started
  "$IMAGE" fim-notify on

  # Compress
  "$IMAGE" fim-commit

  # Copy
  rm -rf ../firefox.flatimage
  mv "$IMAGE" ../firefox.flatimage
}

_main "$@"
