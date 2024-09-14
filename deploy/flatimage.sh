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
export FIM_IMG_SIZE_OFFSET="${FIM_IMG_SIZE_OFFSET:=50}"

FIM_DIR_SCRIPT=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
FIM_DIR="$(dirname "$(dirname -- "$FIM_DIR_SCRIPT")")"
FIM_DIR_BUILD="$FIM_DIR"/build

# shellcheck source=/dev/null
source "${FIM_DIR_SCRIPT}/_common.sh"

function _fetch_static()
{
  # Fetch coreutils
  wget "https://github.com/ruanformigoni/coreutils-static/releases/download/d7f4cd2/coreutils-x86_64.tar.xz"

  # Extracts a bin/... folder
  tar xf "coreutils-x86_64.tar.xz"

  # Fetch Magick
  wget -O bin/magick "https://github.com/ruanformigoni/imagemagick-static-musl/releases/download/c1c5775/magick-x86_64"
  wget -O bin/magick-license "https://raw.githubusercontent.com/ImageMagick/ImageMagick/main/LICENSE"
  chmod +x bin/magick

  # Fetch lsof
  wget -O./bin/lsof "https://github.com/ruanformigoni/lsof-static-musl/releases/download/12c2552/lsof-x86_64"

  # Fetch e2fsprogs
  wget -O ./bin/fuse2fs   "https://github.com/ruanformigoni/e2fsprogs/releases/download/8bd2cc1/fuse2fs-x86_64"
  wget -O ./bin/mke2fs    "https://github.com/ruanformigoni/e2fsprogs/releases/download/8bd2cc1/mke2fs-x86_64"
  wget -O ./bin/e2fsck    "https://github.com/ruanformigoni/e2fsprogs/releases/download/8bd2cc1/e2fsck-x86_64"
  wget -O ./bin/resize2fs "https://github.com/ruanformigoni/e2fsprogs/releases/download/8bd2cc1/resize2fs-x86_64"

  # Fetch proot
  wget -O ./bin/proot "https://github.com/ruanformigoni/proot/releases/download/d9211c8/proot-x86_64"

  # Fetch bwrap
  wget -O ./bin/bwrap "https://github.com/ruanformigoni/bubblewrap-musl-static/releases/download/719925f/bwrap-x86_64"

  # Fetch overlayfs
  wget -O ./bin/overlayfs "https://github.com/ruanformigoni/fuse-overlayfs/releases/download/bc2814e/fuse-overlayfs-x86_64"

  # Fetch dwarfs
  wget -O bin/dwarfs_aio "https://github.com/mhx/dwarfs/releases/download/v0.9.8/dwarfs-universal-0.9.8-Linux-x86_64-clang"
  ln -sf dwarfs_aio bin/mkdwarfs
  ln -sf dwarfs_aio bin/dwarfsextract
  ln -sf dwarfs_aio bin/dwarfs

  # Fetch bash
  wget -O ./bin/bash "https://github.com/ruanformigoni/bash-static/releases/download/b604d6c/bash-x86_64"

  # Setup xdg scripts
  cp "$FIM_DIR"/src/xdg/xdg-* ./bin

  # Make binaries executable
  chmod +x ./bin/*
}

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
  tune2fs -m 1 "$out"
}

# Concatenates binary files and filesystem to create fim image
# $1 = Path to system image
# $2 = Output file name
function _create_elf()
{
  local img="$1"
  local out="$2"

  cp bin/boot "$out"
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
    
  [[ "$dist" =~ bionic|focal|jammy|lunar ]] || _die "Invalid distribution $1"

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
  cat "$FIM_DIR_SCRIPT/../../sources/apt.list" | sed "s|DISTRO|$dist|" | tee "/tmp/$dist/etc/apt/sources.list"

  # Update packages
  pkglist=(
	  'rsync'
	  'alsa-utils'
	  'alsa-base'
	  'alsa-oss'
	  'pulseaudio'
	  'libc6'
	  'libc6-i386'
	  'libpaper1'
	  'fontconfig-config'
	  'ppp'
	  'man-db'
	  'libnss-mdns'
	  'gdm3'
  )

  # Ubuntu 20.04 and earlier use 'ippusbxd', but 'ipp-usb' is used as a replacement in all further releases.
  case "$dist" in
    bionic|focal) pkglist+=('ippusbxd') ;;
    *)            pkglist+=('ipp-usb') ;;
  esac

  chroot "/tmp/$dist" /bin/bash -c 'apt -y update && apt -y upgrade'
  chroot "/tmp/$dist" /bin/bash -c 'apt -y clean && apt -y autoclean && apt -y autoremove'
  chroot "/tmp/$dist" /bin/bash -c "apt install -y ${pkglist[*]}"

  # Umount binds
  umount  "/tmp/$dist/proc/"
  umount  "/tmp/$dist/sys/"
  umount -R "/tmp/$dist/dev/"

  # Remove mount dirs that may have leftover files
  rm -rf /tmp/"$dist"/{tmp,proc,sys,dev,run}

  # Create required mount point folders
  mkdir -p /tmp/"$dist"/{tmp,proc,sys,dev,run/media,mnt,media,home}

  # Create required files for later binding
  rm -f /tmp/"$dist"/etc/{host.conf,hosts,passwd,group,nsswitch.conf,resolv.conf}
  touch /tmp/"$dist"/etc/{host.conf,hosts,passwd,group,nsswitch.conf,resolv.conf}

  # Create share symlink
  ln -s /usr/share "/tmp/$dist/share"

  # Create fim dir
  mkdir -p "/tmp/$dist/fim"

  # Create mounts symlink
  ln -sf /tmp/fim/run/mounts/symlinks "/tmp/$dist/fim/mount"

  # Create fim dwarfs dir
  mkdir -p "/tmp/$dist/fim/dwarfs"

  # Create fim tools dir
  mkdir -p "/tmp/$dist/fim/static"

  # Embed static binaries
  cp -r ./bin/* "/tmp/$dist/fim/static"

  # Embed runner
  cp "$FIM_DIR_SCRIPT/_boot.sh" "/tmp/$dist/fim/boot"

  # Embed permissions
  cp "$FIM_DIR_SCRIPT/_perms.sh" "/tmp/$dist/fim/perms"

  # Set permissions
  chown -R "$(id -u)":users "/tmp/$dist"
  chmod 777 -R "/tmp/$dist"

  # MIME
  mkdir -p "/tmp/$dist/fim/desktop"
  cp ./mime/icon.svg      "/tmp/$dist/fim/desktop"
  cp ./mime/flatimage.xml "/tmp/$dist/fim/desktop"

  # Create image
  _create_image  "/tmp/$dist" "$dist.img"

  # Create elf
  _create_elf "$dist.img" "$dist.flatimage"

  # Create sha256sum
  sha256sum "$dist.flatimage" > dist/"$dist.flatimage.sha256sum"

  tar -cf "$dist.tar" "$dist.flatimage"
  xz -3zv "$dist.tar"
  sha256sum "$dist.tar.xz" > dist/"$dist.tar.xz.sha256sum"

  mv "$dist.tar.xz" dist/
}

# Creates an empty subsystem
function _create_subsystem_empty()
{
  local dist="empty"

  mkdir -p dist

  # Fetch static binaries, creates a bin folder
  _fetch_static

  # Create fim dir
  mkdir -p "/tmp/$dist/fim"

  # Create config dir
  mkdir -p "/tmp/$dist/fim/config"

  # Embed static binaries
  mkdir -p "/tmp/$dist/fim/static"
  cp -r ./bin/* "/tmp/$dist/fim/static"

  # Compile and include runner
  (
    cd "$FIM_DIR"
    docker build . --build-arg FIM_DIR="$(pwd)" -t flatimage-boot -f docker/Dockerfile.boot
    docker run -it --rm -v "$FIM_DIR_BUILD":"/host" flatimage-boot cp "$FIM_DIR"/src/boot/build/Release/boot /host/bin
    docker run -it --rm -v "$FIM_DIR_BUILD":"/host" flatimage-boot cp "$FIM_DIR"/src/boot/janitor /host/bin
  )

  # Compile and include portal
  (
    cd "$FIM_DIR"
    docker build . -t "flatimage-portal" -f docker/Dockerfile.portal
    docker run --rm -v "$FIM_DIR_BUILD":/host "flatimage-portal" cp /fim/dist/fim_portal /fim/dist/fim_portal_daemon /host/bin
  )

  cp ./bin/fim_portal ./bin/fim_portal_daemon           /tmp/"$dist"/fim/static
  cp ./bin/boot       /tmp/"$dist"/fim/static/boot
  cp ./bin/janitor    /tmp/"$dist"/fim/static/janitor

  # Set permissions
  chown -R "$(id -u)":users "/tmp/$dist"
  chmod 777 -R "/tmp/$dist"

  # MIME
  mkdir -p "/tmp/$dist/fim/desktop"
  cp "$FIM_DIR"/mime/icon.svg      "/tmp/$dist/fim/desktop"
  cp "$FIM_DIR"/mime/flatimage.xml "/tmp/$dist/fim/desktop"

  # Create root filesystem and layers folder
  mkdir ./root
  mv /tmp/"$dist"/fim ./root
  mkdir ./root/fim/layers

  # Create image
  _create_image ./root "$dist.img"

  # Create elf
  _create_elf "$dist.img" "$dist.flatimage"

  # Create sha256sum
  sha256sum "$dist.flatimage" > dist/"$dist.flatimage.sha256sum"

  tar -cf "$dist.tar" "$dist.flatimage"
  xz -3zv "$dist.tar"
  sha256sum "$dist.tar.xz" > dist/"$dist.tar.xz.sha256sum"

  mv "$dist.tar.xz" dist/
}

# Creates an alpine subsystem
# Requires root permissions
function _create_subsystem_alpine()
{
  mkdir -p dist

  # Fetch static binaries, creates a bin folder
  _fetch_static

  local dist="alpine"

  # Update exec permissions
  chmod -R +x ./bin/.

  # Build
  rm -rf /tmp/"$dist"
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
  mkdir -p /tmp/"$dist"/{tmp,proc,sys,dev,run/media,mnt,media,home}

  # Create required files for later binding
  rm -f /tmp/"$dist"/etc/{host.conf,hosts,passwd,group,nsswitch.conf,resolv.conf}
  touch /tmp/"$dist"/etc/{host.conf,hosts,passwd,group,nsswitch.conf,resolv.conf}

  # Update packages
  chmod +x ./bin/proot
  ./bin/proot -R "/tmp/$dist" /bin/sh -c 'apk update'
  ./bin/proot -R "/tmp/$dist" /bin/sh -c 'apk upgrade'
  ./bin/proot -R "/tmp/$dist" /bin/sh -c 'apk add bash alsa-utils alsa-utils-doc alsa-lib alsaconf alsa-ucm-conf pulseaudio pulseaudio-alsa' || true

  # Create fim dir
  mkdir -p "/tmp/$dist/fim"

  # Create config dir
  mkdir -p "/tmp/$dist/fim/config"

  # Embed static binaries
  mkdir -p "/tmp/$dist/fim/static"
  cp -r ./bin/* "/tmp/$dist/fim/static"

  # Compile and include runner
  (
    cd "$FIM_DIR"
    docker build . --build-arg FIM_DIR="$(pwd)" -t flatimage-boot -f docker/Dockerfile.boot
    docker run -it --rm -v "$FIM_DIR_BUILD":"/host" flatimage-boot cp "$FIM_DIR"/src/boot/build/Release/boot /host/bin
    docker run -it --rm -v "$FIM_DIR_BUILD":"/host" flatimage-boot cp "$FIM_DIR"/src/boot/janitor /host/bin
  )

  # Compile and include portal
  (
    cd "$FIM_DIR"
    docker build . -t "flatimage-portal" -f docker/Dockerfile.portal
    docker run --rm -v "$FIM_DIR_BUILD":/host "flatimage-portal" cp /fim/dist/fim_portal /fim/dist/fim_portal_daemon /host/bin
  )

  cp ./bin/fim_portal ./bin/fim_portal_daemon           /tmp/"$dist"/fim/static
  cp ./bin/boot       /tmp/"$dist"/fim/static/boot
  cp ./bin/janitor    /tmp/"$dist"/fim/static/janitor

  # Set permissions
  chown -R "$(id -u)":users "/tmp/$dist"
  chmod 777 -R "/tmp/$dist"

  # MIME
  mkdir -p "/tmp/$dist/fim/desktop"
  cp "$FIM_DIR"/mime/icon.svg      "/tmp/$dist/fim/desktop"
  cp "$FIM_DIR"/mime/flatimage.xml "/tmp/$dist/fim/desktop"

  # Create root filesystem and layers folder
  mkdir ./root
  mv /tmp/"$dist"/fim ./root
  mkdir ./root/fim/layers

  # Create layer 0 compressed filesystem
  "$FIM_DIR_BUILD"/bin/mkdwarfs -l 7 -i /tmp/"$dist" -o ./alpine.dwarfs
  rm -rf /tmp/"$dist"

  # Change filesystem name to index:sha
  mv ./alpine.dwarfs ./root/fim/layers/"0-$(sha256sum ./alpine.dwarfs | awk '{print $1}')"

  # Create image
  _create_image ./root "$dist.img"

  # Create elf
  _create_elf "$dist.img" "$dist.flatimage"

  # Create sha256sum
  sha256sum "$dist.flatimage" > dist/"$dist.flatimage.sha256sum"

  tar -cf "$dist.tar" "$dist.flatimage"
  xz -3zv "$dist.tar"
  sha256sum "$dist.tar.xz" > dist/"$dist.tar.xz.sha256sum"

  mv "$dist.tar.xz" dist/
}

# Creates an arch subsystem
# Requires root permissions
function _create_subsystem_arch()
{
  mkdir -p dist

  # Fetch static binaries, creates a bin folder
  _fetch_static

  # Fetch bootstrap
  git clone "https://github.com/ruanformigoni/arch-bootstrap.git"

  # Build
  sed -i 's|DEFAULT_REPO_URL=".*"|DEFAULT_REPO_URL="http://linorg.usp.br/archlinux"|' ./arch-bootstrap/arch-bootstrap.sh
  sed -Ei 's|^\s+curl|curl --retry 5|' ./arch-bootstrap/arch-bootstrap.sh
  sed 's/^/-- /' ./arch-bootstrap/arch-bootstrap.sh
  ./arch-bootstrap/arch-bootstrap.sh arch

  # Update mirrorlist
  cp "$FIM_DIR_SCRIPT/../../sources/arch.list" arch/etc/pacman.d/mirrorlist

  # Enable multilib
  gawk -i inplace '/#\[multilib\]/,/#Include = (.*)/ { sub("#", ""); } 1' ./arch/etc/pacman.conf
  chroot arch /bin/bash -c "pacman -Sy"

  # Audio & video
  pkgs_va+=("alsa-lib lib32-alsa-lib alsa-plugins lib32-alsa-plugins libpulse")
  pkgs_va+=("lib32-libpulse alsa-tools alsa-utils")
  pkgs_va+=("pipewire lib32-pipewire pipewire-pulse")
  pkgs_va+=("pipewire-jack lib32-pipewire-jack pipewire-alsa")
  pkgs_va+=("wireplumber")
  # pkgs_va+=("mesa lib32-mesa vulkan-radeon lib32-vulkan-radeon vulkan-intel nvidia-utils")
  # pkgs_va+=("lib32-vulkan-intel lib32-nvidia-utils vulkan-icd-loader lib32-vulkan-icd-loader")
  # pkgs_va+=("vulkan-mesa-layers lib32-vulkan-mesa-layers libva-mesa-driver")
  # pkgs_va+=("lib32-libva-mesa-driver libva-intel-driver lib32-libva-intel-driver")
  # pkgs_va+=("intel-media-driver mesa-utils vulkan-tools nvidia-prime libva-utils")
  # pkgs_va+=("lib32-mesa-utils")
  chroot arch /bin/bash -c "pacman -S ${pkgs_va[*]} --noconfirm"

  # Required for nvidia
  chroot arch /bin/bash -c "pacman -S --noconfirm libvdpau lib32-libvdpau"

  # Avoid segfaults on some OpenGL applications
  chroot arch /bin/bash -c "pacman -S --noconfirm libxkbcommon lib32-libxkbcommon"

  # Pacman hooks
  ## Avoid taking too long on 'Arming ConditionNeedsUpdate' and 'Update MIME database'
  { sed -E 's/^\s+://' | tee ./arch/patch.sh | sed -e 's/^/-- /'; } <<-"END"
  :#!/usr/bin/env bash
  :shopt -s nullglob
  :for i in $(grep -rin -m1 -l "ConditionNeedsUpdate" /usr/lib/systemd/system/); do
  :  sed -Ei "s/ConditionNeedsUpdate=.*/ConditionNeedsUpdate=/" "$i"
  :done
  :grep -rin "ConditionNeedsUpdate" /usr/lib/systemd/system/
  :
  :mkdir -p /etc/pacman.d/hooks
  :
  :tee /etc/pacman.d/hooks/{update-desktop-database.hook,30-update-mime-database.hook} <<-"END"
  :[Trigger]
  :Type = Path
  :Operation = Install
  :Operation = Upgrade
  :Operation = Remove
  :Target = *

  :[Action]
  :Description = Overriding the desktop file MIME type cache...
  :When = PostTransaction
  :Exec = /bin/true
  :END
  :
  :tee /etc/pacman.d/hooks/cleanup-pkgs.hook <<-"END"
  :[Trigger]
  :Type = Path
  :Operation = Install
  :Operation = Upgrade
  :Operation = Remove
  :Target = *

  :[Action]
  :Description = Cleaning up downloaded files...
  :When = PostTransaction
  :Exec = /bin/sh -c 'rm -rf /var/cache/pacman/pkg/*'
  :END
  :
  :tee /etc/pacman.d/hooks/cleanup-locale.hook <<-"END"
  :[Trigger]
  :Type = Path
  :Operation = Install
  :Operation = Upgrade
  :Operation = Remove
  :Target = *

  :[Action]
  :Description = Cleaning up locale files...
  :When = PostTransaction
  :Exec = /bin/sh -c 'find /usr/share/locale -mindepth 1 -maxdepth 1 -type d -not -iname "en_us" -exec rm -rf "{}" \;'
  :END
  :
  :tee /etc/pacman.d/hooks/cleanup-doc.hook <<-"END"
  :[Trigger]
  :Type = Path
  :Operation = Install
  :Operation = Upgrade
  :Operation = Remove
  :Target = *

  :[Action]
  :Description = Cleaning up doc...
  :When = PostTransaction
  :Exec = /bin/sh -c 'rm -rf /usr/share/doc/*'
  :END
  :
  :tee /etc/pacman.d/hooks/cleanup-man.hook <<-"END"
  :[Trigger]
  :Type = Path
  :Operation = Install
  :Operation = Upgrade
  :Operation = Remove
  :Target = *

  :[Action]
  :Description = Cleaning up man...
  :When = PostTransaction
  :Exec = /bin/sh -c 'rm -rf /usr/share/man/*'
  :END
  :
  :tee /etc/pacman.d/hooks/cleanup-fonts.hook <<-"END"
  :[Trigger]
  :Type = Path
  :Operation = Install
  :Operation = Upgrade
  :Operation = Remove
  :Target = *

  :[Action]
  :Description = Cleaning up noto fonts...
  :When = PostTransaction
  :Exec = /bin/sh -c 'find /usr/share/fonts/noto -mindepth 1 -type f -not -iname "notosans-*" -and -not -iname "notoserif-*" -exec rm "{}" \;'
  :END
  :
	END
  chmod +x ./arch/patch.sh
  chroot arch /bin/bash -c /patch.sh
  rm ./arch/patch.sh

  # Input
  chroot arch /bin/bash -c "pacman -S libusb sdl2 lib32-sdl2 --noconfirm"

  # fakeroot & fakechroot
  chroot arch /bin/bash -c "pacman -S git fakeroot fakechroot binutils --noconfirm"

  # Clear cache
  chroot arch /bin/bash -c "pacman -Scc --noconfirm"
  rm -rf ./arch/var/cache/pacman/pkg/*

  # Configure Locale
  sed -i 's/#en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' ./arch/etc/locale.gen
  chroot arch /bin/bash -c "locale-gen"
  echo "LANG=en_US.UTF-8" > ./arch/etc/locale.conf

  # Create share symlink
  ln -sf /usr/share ./arch/share

  # Create fim dir
  mkdir -p "./arch/fim"

  # Create config dir
  mkdir -p "./arch/fim/config"

  # Compile and include runner
  (
    cd "$FIM_DIR"
    docker build . --build-arg FIM_DIR="$(pwd)" -t flatimage-boot -f docker/Dockerfile.boot
    docker run -it --rm -v "$FIM_DIR_BUILD":"/host" flatimage-boot cp "$FIM_DIR"/src/boot/build/Release/boot /host/bin
    docker run -it --rm -v "$FIM_DIR_BUILD":"/host" flatimage-boot cp "$FIM_DIR"/src/boot/janitor /host/bin
  )

  # Compile and include portal
  (
    cd "$FIM_DIR"
    docker build . -t "flatimage-portal" -f docker/Dockerfile.portal
    docker run --rm -v "$FIM_DIR_BUILD":/host "flatimage-portal" cp /fim/dist/fim_portal /fim/dist/fim_portal_daemon /host/bin
  )

  # Embed static binaries
  mkdir -p "./arch/fim/static"
  cp -r "$FIM_DIR_BUILD"/bin/* "./arch/fim/static"

  # Remove mount dirs that may have leftover files
  rm -rf arch/{tmp,proc,sys,dev,run}

  # Create required mount points if not exists
  mkdir -p arch/{tmp,proc,sys,dev,run/media,mnt,media,home}

  # Create required files for later binding
  rm -f arch/etc/{host.conf,hosts,passwd,group,nsswitch.conf,resolv.conf}
  touch arch/etc/{host.conf,hosts,passwd,group,nsswitch.conf,resolv.conf}

  # Set permissions
  chown -R "$(id -u)":users "./arch"
  chmod 755 -R "./arch"

  # MIME
  mkdir -p ./arch/fim/desktop
  cp "$FIM_DIR"/mime/icon.svg      ./arch/fim/desktop
  cp "$FIM_DIR"/mime/flatimage.xml ./arch/fim/desktop

  # Create root filesystem and layers folder
  mkdir ./root
  mv ./arch/fim ./root
  mkdir ./root/fim/layers

  # Create layer 0 compressed filesystem
  "$FIM_DIR_BUILD"/bin/mkdwarfs -l 7 -i ./arch -o ./arch.dwarfs
  rm -rf ./arch

  # Change filesystem name to index:sha
  mv arch.dwarfs ./root/fim/layers/"0-$(sha256sum ./arch.dwarfs | awk '{print $1}')"

  # Create image
  _create_image  "./root" "arch.img"

  # Create elf
  _create_elf "arch.img" "arch.flatimage"

  # Create sha256sum
  sha256sum arch.flatimage > dist/"arch.flatimage.sha256sum"

  tar -cf arch.tar arch.flatimage
  xz -3zv arch.tar
  sha256sum arch.tar.xz > dist/"arch.tar.xz.sha256sum"

  mv "arch.tar.xz" dist/
}

function main()
{
  rm -rf "$FIM_DIR_BUILD"
  mkdir "$FIM_DIR_BUILD"
  cd "$FIM_DIR_BUILD"

  case "$1" in
    "debian")   _create_subsystem_debootstrap "${@:2}" ;;
    "arch") _create_subsystem_arch ;;
    "alpine") _create_subsystem_alpine ;;
    "empty") _create_subsystem_empty ;;
    *) _die "Invalid option $2" ;;
  esac
}

main "$@"
