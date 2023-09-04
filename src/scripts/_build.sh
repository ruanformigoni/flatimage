#!/usr/bin/env bash

######################################################################
# @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
# @file        : _build
# @created     : Monday Jan 23, 2023 21:18:26 -03
#
# @description : Tools to build the filesystem
######################################################################

#shellcheck disable=2155

set -e

# Max size (M) = actual occupied size + offset
export ARTS_IMG_SIZE_OFFSET="${ARTS_IMG_SIZE_OFFSET:=50}"

ARTS_SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# shellcheck source=/dev/null
source "${ARTS_SCRIPT_DIR}/_common.sh"

# Creates a filesystem image
# $1 = folder to create image from
# $2 = output file name
function _create_image()
{
  # Check inputs
  [ -d "$1" ] || _die "Invalid directory $1"
  [ -n "$2" ] || _die "Empty output file name"

  # Set vars & max size
  local dir="$1"
  local out="$(basename "$2")"
  local slack="50" # 50M
  local size="$(du -s "$dir" | awk '{printf "%d\n", $1/1000}')"
  size="$((size+slack))M"

  _msg "dir            : $dir"
  _msg "file out       : $out"
  _msg "size dir+slack : $size"

  # Create filesystem
  truncate -s "$size" "$out"
  bin/mke2fs -d "$dir" -b1024 -t ext2 "$out"
}

# Concatenates binary files and filesystem to create arts image
# $1 = Path to system image
# $2 = Output file name
function _create_elf()
{
  local img="$1"
  local out="$2"

  cp bin/elf "$out"
  cat bin/{fuse2fs,e2fsck,bash} >> "$out"
  cat "$img" >> "$out"

}

# Extracts the filesystem of a docker image
# $1 = Dockerfile
# $2 = Docker Tag, e.g, ubuntu:subsystem
# $3 = Output tarball name
function _create_subsystem_docker()
{
  local dockerfile="$1"
  local tag="$2"
  local out="$3"

  [ -f "$1" ] || _die "Invalid dockerfile $1"
  [ -n "$2" ] || _die "Empty tag parameter"
  [ -n "$3" ] || _die "Empty output filename"

  _msg "dockerfile: $(readlink -f "$dockerfile")"
  _msg "tag: $tag"
  _msg "outfile: $out"

  # Build image
  docker build -f "$dockerfile" -t "$tag" "$(dirname "$dockerfile")"
  # Start docker
  docker run --rm -t "$tag" sleep 10 &
  sleep 5 # Wait for mount
  # Extract subsystem
  local container="$(docker container list | grep "$tag" | awk '{print $1}')"
  # Create subsystem snapshot
  docker export "$container" > "${out%.tar}.tar"
  # Finish background job
  kill %1
}

# Creates a subsystem with debootstrap
# Requires root permissions
# $1 = Distribution, e.g., bionic, focal, jammy
function _create_subsystem_debootstrap()
{
  mkdir -p dist
  mkdir -p bin

  local dist="$1"

  [[ "$dist" =~ bionic|focal|jammy ]] || _die "Invalid distribution $1"

  # Update exec permissions
  chmod -R +x ./bin/.

  # Build
  debootstrap "$dist" "/tmp/$dist" http://archive.ubuntu.com/ubuntu/

  # Bind mounts
  mount -t proc /proc "/tmp/$dist/proc/"
  mount -t sysfs /sys "/tmp/$dist/sys/"
  mount --rbind /dev "/tmp/$dist/dev/"
  mount --make-rslave "/tmp/$dist/dev/"

  # Update sources
  # shellcheck disable=2002
  cat "$ARTS_SCRIPT_DIR/../../sources/${dist}.list" | tee "/tmp/$dist/etc/apt/sources.list"

  # Update packages
  chroot "/tmp/$dist" /bin/bash -c 'apt -y update && apt -y upgrade'
  chroot "/tmp/$dist" /bin/bash -c 'apt -y clean && apt -y autoclean && apt -y autoremove'
  chroot "/tmp/$dist" /bin/bash -c 'apt install -y rsync alsa-utils alsa-base alsa-oss pulseaudio libc6 libc6-i386 libpaper1 fontconfig-config ppp man-db libnss-mdns ippusbxd gdm3'

  # Umount binds
  umount  "/tmp/$dist/proc/"
  umount  "/tmp/$dist/sys/"
  umount -R "/tmp/$dist/dev/"

  # Remove mount dirs that may have leftover files
  rm -rf /tmp/"$dist"/{tmp,proc,sys,dev,run}

  # Create required mount point folders
  mkdir -p /tmp/"$dist"/{tmp,proc,sys,dev,run,home}

  # Create required files for later binding
  rm -f /tmp/"$dist"/etc/{host.conf,hosts,passwd,group,nsswitch.conf,resolv.conf}
  touch /tmp/"$dist"/etc/{host.conf,hosts,passwd,group,nsswitch.conf,resolv.conf}

  # Create share symlink
  ln -s /usr/share "/tmp/$dist/share"

  # Create arts tools dir
  mkdir -p "/tmp/$dist/arts/static"

  # Embed static binaries
  cp -r ./bin/* "/tmp/$dist/arts/static"

  # Embed runner
  cp "$ARTS_SCRIPT_DIR/_boot.sh" "/tmp/$dist/arts/boot"

  # Embed permissions
  cp "$ARTS_SCRIPT_DIR/_perms.sh" "/tmp/$dist/arts/perms"

  # Set permissions
  chown -R "$(id -u)":users "/tmp/$dist"
  chmod 777 -R "/tmp/$dist"

  # Create image
  _create_image  "/tmp/$dist" "$dist.img"

  # Create elf
  _create_elf "$dist.img" "$dist.arts"

  tar -cf "$dist.tar" "$dist.arts"
  xz -3zv "$dist.tar"

  mv "$dist.tar.xz" dist/
}

# Creates an alpine subsystem
# Requires root permissions
function _create_subsystem_alpine()
{
  mkdir -p dist
  mkdir -p bin

  local dist="alpine"

  # Update exec permissions
  chmod -R +x ./bin/.

  # Build
  wget http://dl-cdn.alpinelinux.org/alpine/v3.12/main/x86_64/apk-tools-static-2.10.8-r1.apk
  tar zxf apk-tools-static-*.apk
  ./sbin/apk.static --arch x86_64 -X http://dl-cdn.alpinelinux.org/alpine/latest-stable/main/ -U --allow-untrusted --root /tmp/"$dist" --initdb add alpine-base

  rm -rf /tmp/"$dist"/dev /tmp/"$dist"/target

  # Touch sources
  { sed -E 's/^\s+://' | tee /tmp/"$dist"/etc/apk/repositories; } <<-END
    :http://dl-cdn.alpinelinux.org/alpine/v3.16/main
    :http://dl-cdn.alpinelinux.org/alpine/v3.16/community
    :#http://dl-cdn.alpinelinux.org/alpine/edge/main
    :#http://dl-cdn.alpinelinux.org/alpine/edge/community
    :#http://dl-cdn.alpinelinux.org/alpine/edge/testing
	END

  # Include additional paths in PATH for proot
  export PATH="$PATH:/sbin:/bin"

  # Remove mount dirs that may have leftover files
  rm -rf /tmp/"$dist"/{tmp,proc,sys,dev,run}

  # Create required mount point folders
  mkdir -p /tmp/"$dist"/{tmp,proc,sys,dev,run,home}

  # Create required files for later binding
  rm -f /tmp/"$dist"/etc/{host.conf,hosts,passwd,group,nsswitch.conf,resolv.conf}
  touch /tmp/"$dist"/etc/{host.conf,hosts,passwd,group,nsswitch.conf,resolv.conf}

  # Update packages
  chmod +x ./bin/proot
  ./bin/proot -R "/tmp/$dist" /bin/sh -c 'apk update'
  ./bin/proot -R "/tmp/$dist" /bin/sh -c 'apk upgrade'
  ./bin/proot -R "/tmp/$dist" /bin/sh -c 'apk add bash alsa-utils alsa-utils-doc alsa-lib alsaconf alsa-ucm-conf pulseaudio pulseaudio-alsa'

  # Create arts tools dir
  mkdir -p "/tmp/$dist/arts/static"

  # Embed static binaries
  cp -r ./bin/* "/tmp/$dist/arts/static"

  # Embed runner
  cp "$ARTS_SCRIPT_DIR/_boot.sh" "/tmp/$dist/arts/boot"

  # Embed permissions
  cp "$ARTS_SCRIPT_DIR/_perms.sh" "/tmp/$dist/arts/perms"

  # Set permissions
  chown -R "$(id -u)":users "/tmp/$dist"
  chmod 777 -R "/tmp/$dist"

  # Create image
  _create_image  "/tmp/$dist" "$dist.img"

  # Create elf
  _create_elf "$dist.img" "$dist.arts"

  tar -cf "$dist.tar" "$dist.arts"
  xz -3zv "$dist.tar"

  mv "$dist.tar.xz" dist/
}

# Creates an arch subsystem
# Requires root permissions
function _create_subsystem_arch()
{
  mkdir -p dist
  mkdir -p bin

  # Update exec permissions
  chmod -R +x ./bin/.

  # Fetch bootstrap
  git clone https://github.com/tokland/arch-bootstrap.git

  # Build
  sed -i 's|DEFAULT_REPO_URL=".*"|DEFAULT_REPO_URL="http://linorg.usp.br/archlinux"|' ./arch-bootstrap/arch-bootstrap.sh
  sed -Ei 's|^\s+curl|curl --retry 5|' ./arch-bootstrap/arch-bootstrap.sh
  sed 's/^/-- /' ./arch-bootstrap/arch-bootstrap.sh
  ./arch-bootstrap/arch-bootstrap.sh arch

  # Update mirrorlist
  cp "$ARTS_SCRIPT_DIR/../../sources/arch.list" arch/etc/pacman.d/mirrorlist

  # Enable multilib
  gawk -i inplace '/#\[multilib\]/,/#Include = (.*)/ { sub("#", ""); } 1' ./arch/etc/pacman.conf
  chroot arch /bin/bash -c "pacman -Sy"

  # Audio & video
  pkgs_va+=("alsa-lib lib32-alsa-lib alsa-plugins lib32-alsa-plugins libpulse")
  pkgs_va+=("lib32-libpulse jack2 lib32-jack2 alsa-tools alsa-utils")
  # pkgs_va+=("mesa lib32-mesa vulkan-radeon lib32-vulkan-radeon vulkan-intel nvidia-utils")
  # pkgs_va+=("lib32-vulkan-intel lib32-nvidia-utils vulkan-icd-loader lib32-vulkan-icd-loader")
  # pkgs_va+=("vulkan-mesa-layers lib32-vulkan-mesa-layers libva-mesa-driver")
  # pkgs_va+=("lib32-libva-mesa-driver libva-intel-driver lib32-libva-intel-driver")
  # pkgs_va+=("intel-media-driver mesa-utils vulkan-tools nvidia-prime libva-utils")
  # pkgs_va+=("lib32-mesa-utils")
  chroot arch /bin/bash -c "pacman -S ${pkgs_va[*]} --noconfirm"

  # Input
  chroot arch /bin/bash -c "pacman -S libusb sdl2 lib32-sdl2 --noconfirm"

  # fakeroot & fakechroot
  chroot arch /bin/bash -c "pacman -S git fakeroot fakechroot binutils --noconfirm"

  # Clear cache
  chroot arch /bin/bash -c "pacman -Scc --noconfirm"
  rm -rf ./arch/var/cache/pacman/pkg/*

  # Install yay
  wget -O yay.tar.gz https://github.com/Jguer/yay/releases/download/v11.3.2/yay_11.3.2_x86_64.tar.gz
  tar -xf yay.tar.gz --strip-components=1 "yay_11.3.2_x86_64/yay"
  rm yay.tar.gz
  mv yay arch/usr/bin

  # Create share symlink
  ln -sf /usr/share ./arch/share

  # Create arts tools dir
  mkdir -p "./arch/arts/static"

  # Embed static binaries
  cp -r ./bin/* "./arch/arts/static"

  # Embed runner
  cp "$ARTS_SCRIPT_DIR/_boot.sh" "./arch/arts/boot"

  # Embed permissions
  cp "$ARTS_SCRIPT_DIR/_perms.sh" "./arch/arts/perms"

  # Embed AUR helper
  cp "$ARTS_SCRIPT_DIR/_aur.sh" "./arch/usr/bin/aur"

  # Remove mount dirs that may have leftover files
  rm -rf arch/{tmp,proc,sys,dev,run}

  # Create required mount points if not exists
  mkdir -p arch/{tmp,proc,sys,dev,run,home}

  # Create required files for later binding
  rm -f arch/etc/{host.conf,hosts,passwd,group,nsswitch.conf,resolv.conf}
  touch arch/etc/{host.conf,hosts,passwd,group,nsswitch.conf,resolv.conf}

  # Set permissions
  chown -R "$(id -u)":users "./arch"
  chmod 755 -R "./arch"

  # Create image
  _create_image  "./arch" "arch.img"

  # Create elf
  _create_elf "arch.img" "arch.arts"

  tar -cf arch.tar arch.arts
  xz -3zv arch.tar

  mv "arch.tar.xz" dist/
}

function main()
{
  case "$1" in
    "debootstrap")   _create_subsystem_debootstrap "${@:2}" ;;
    "archbootstrap") _create_subsystem_arch ;;
    "alpinebootstrap") _create_subsystem_alpine ;;
    *) _die "Invalid option $2" ;;
  esac
}

main "$@"
