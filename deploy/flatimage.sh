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

FIM_DIR_SCRIPT=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
FIM_DIR="$(dirname "$FIM_DIR_SCRIPT")"
FIM_DIR_BUILD="$FIM_DIR"/build

# shellcheck source=/dev/null
source "${FIM_DIR_SCRIPT}/common.sh"

function _fetch_static()
{
  mkdir -p bin

  # Fetch busybox
  wget -O ./bin/busybox "https://github.com/ruanformigoni/busybox-static-musl/releases/download/7e2c5b6/busybox-x86_64"

  # Fetch lsof
  wget -O./bin/lsof "https://github.com/ruanformigoni/lsof-static-musl/releases/download/720c914/lsof-x86_64"

  # Fetch bwrap
  wget -O ./bin/bwrap "https://github.com/ruanformigoni/bubblewrap-musl-static/releases/download/b881755/bwrap-x86_64"

  # Fetch proot
  wget -O ./bin/proot "https://github.com/ruanformigoni/proot-static-musl/releases/download/bf747c8/proot-x86_64"

  # Fetch overlayfs
  wget -O ./bin/overlayfs "https://github.com/ruanformigoni/fuse-overlayfs-static-musl/releases/download/1861741/fuse-overlayfs-x86_64"

  # Fetch ciopfs
  wget -O ./bin/ciopfs "https://github.com/ruanformigoni/ciopfs-static-musl/releases/download/39d5d5a/ciopfs-x86_64"
  # cp "$HOME"/Repositories/ciopfs/ciopfs ./bin/ciopfs

  # Fetch squashfuse
  # wget -O ./bin/squashfuse "https://github.com/ruanformigoni/squashfuse-static-musl/releases/download/f2b4067/squashfuse-x86_64"

  # Fetch dwarfs
  wget -O bin/dwarfs_aio "https://github.com/ruanformigoni/dwarfs/releases/download/187a5b65/dwarfs-universal"
  ln -s dwarfs_aio bin/mkdwarfs
  ln -s dwarfs_aio bin/dwarfs

  # Fetch bash
  wget -O ./bin/bash "https://github.com/ruanformigoni/bash-static-musl/releases/download/b604d6c/bash-x86_64"

  # # Setup xdg scripts
  # cp "$FIM_DIR"/src/xdg/xdg-* ./bin

  # Make binaries executable
  chmod 755 ./bin/*

  # Compress binaries
  upx -6 --no-lzma bin/* || true

  # Create symlinks
  (
    cd bin
    for i in $(./busybox --list); do
      if ! [[ -e "$i" ]]; then
        ln -vs busybox "$i"
      fi
    done
  )
}

# Concatenates binary files and filesystem to create fim image
# $1 = Path to system image
# $2 = Output file name
function _create_elf()
{
  local img="$1"
  local out="$2"

  # Boot is the program on top of the image
  cp bin/boot "$out"
  # Append binaries
  for binary in bin/{bash,busybox,bwrap,ciopfs,dwarfs_aio,fim_portal,fim_portal_daemon,fim_bwrap_apparmor,janitor,lsof,overlayfs,proot}; do
    hex_size_binary="$( du -b "$binary" | awk '{print $1}' | xargs -I{} printf "%016x\n" {} )"
    # Write binary size
    for byte_index in $(seq 0 7 | sort -r); do
      local byte="${hex_size_binary:$(( byte_index * 2)):2}"
      echo -ne "\\x$byte" >> "$out"
    done
    # Append binary
    cat "$binary" >> "$out"
  done
  # Create reserved space
  dd if=/dev/zero of="$out" bs=1 count=2097152 oflag=append conv=notrunc
  # Write size of image rightafter
  size_img="$( du -b "$img" | awk '{print $1}' | xargs -I{} printf "%016x\n" {} )"
  for byte_index in $(seq 0 7 | sort -r); do
    local byte="${size_img:$(( byte_index * 2)):2}"
    echo -ne "\\x$byte" >> "$out"
  done
  # Write image
  cat "$img" >> "$out"

}

# Creates an blueprint subsystem
function _create_subsystem_blueprint()
{
  local dist="blueprint"

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
    docker build . --build-arg FIM_DIST=BLUEPRINT --build-arg FIM_DIR="$(pwd)" -t flatimage-boot -f docker/Dockerfile.boot
    docker run --rm -v "$FIM_DIR_BUILD":"/host" flatimage-boot cp "$FIM_DIR"/src/boot/build/Release/boot /host/bin
    docker run --rm -v "$FIM_DIR_BUILD":"/host" flatimage-boot cp "$FIM_DIR"/src/boot/janitor /host/bin
  )

  # Compile and include portal
  (
    cd "$FIM_DIR"
    docker build . -t "flatimage-portal" -f docker/Dockerfile.portal
    docker run --rm -v "$FIM_DIR_BUILD":/host "flatimage-portal" cp /fim/dist/fim_portal /fim/dist/fim_portal_daemon /host/bin
  )

  # Compile and include bwrap-apparmor
  (
    cd "$FIM_DIR"
    docker build . -t flatimage-bwrap-apparmor -f docker/Dockerfile.bwrap_apparmor
    docker run --rm -v "$FIM_DIR_BUILD":/host "flatimage-bwrap-apparmor" cp /fim/dist/fim_bwrap_apparmor /host/bin
  )

  cp ./bin/fim_portal ./bin/fim_portal_daemon           /tmp/"$dist"/fim/static
  cp ./bin/boot       /tmp/"$dist"/fim/static/boot
  cp ./bin/janitor    /tmp/"$dist"/fim/static/janitor

  # Set permissions
  chown -R 1000:1000 "/tmp/$dist"

  # MIME
  mkdir -p "/tmp/$dist/fim/desktop"
  cp "$FIM_DIR"/mime/icon.svg      "/tmp/$dist/fim/desktop"
  cp "$FIM_DIR"/mime/flatimage.xml "/tmp/$dist/fim/desktop"

  # Create root filesystem directory
  mkdir ./root
  mv /tmp/"$dist"/fim ./root

  # Create image
  # mksquashfs ./root "$dist.img" -comp zstd -Xcompression-level 15
  ./bin/mkdwarfs -i ./root -o "$dist.img"

  # Create elf
  _create_elf "$dist.img" "$dist.flatimage"

  # Create sha256sum
  sha256sum "$dist.flatimage" > ./dist/"$dist.flatimage.sha256sum"
  mv "$dist.flatimage" ./dist
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

  # Include additional paths in PATH
  export PATH="$PATH:/sbin:/bin"

  # Remove mount dirs that may have leftover files
  rm -rf /tmp/"$dist"/{tmp,proc,sys,dev,run}

  # Create required mount point folders
  mkdir -p /tmp/"$dist"/{tmp,proc,sys,dev,run/media,mnt,media,home}

  # Create required files for later binding
  rm -f /tmp/"$dist"/etc/{host.conf,hosts,passwd,group,nsswitch.conf,resolv.conf}
  touch /tmp/"$dist"/etc/{host.conf,hosts,passwd,group,nsswitch.conf,resolv.conf}

  # Update packages
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
    docker build . --build-arg FIM_DIST=ALPINE --build-arg FIM_DIR="$(pwd)" -t flatimage-boot -f docker/Dockerfile.boot
    docker run --rm -v "$FIM_DIR_BUILD":"/host" flatimage-boot cp "$FIM_DIR"/src/boot/build/Release/boot /host/bin
    docker run --rm -v "$FIM_DIR_BUILD":"/host" flatimage-boot cp "$FIM_DIR"/src/boot/janitor /host/bin
  )

  # Compile and include portal
  (
    cd "$FIM_DIR"
    docker build . -t "flatimage-portal" -f docker/Dockerfile.portal
    docker run --rm -v "$FIM_DIR_BUILD":/host "flatimage-portal" cp /fim/dist/fim_portal /fim/dist/fim_portal_daemon /host/bin
  )

  # Compile and include bwrap-apparmor
  (
    cd "$FIM_DIR"
    docker build . -t flatimage-bwrap-apparmor -f docker/Dockerfile.bwrap_apparmor
    docker run --rm -v "$FIM_DIR_BUILD":/host "flatimage-bwrap-apparmor" cp /fim/dist/fim_bwrap_apparmor /host/bin
  )

  cp ./bin/fim_portal ./bin/fim_portal_daemon           /tmp/"$dist"/fim/static
  cp ./bin/boot       /tmp/"$dist"/fim/static/boot
  cp ./bin/janitor    /tmp/"$dist"/fim/static/janitor

  # Set permissions
  chown -R 1000:1000 "/tmp/$dist"

  # MIME
  mkdir -p "/tmp/$dist/fim/desktop"
  cp "$FIM_DIR"/mime/icon.svg      "/tmp/$dist/fim/desktop"
  cp "$FIM_DIR"/mime/flatimage.xml "/tmp/$dist/fim/desktop"

  # Create layer 0 compressed filesystem
  chown -R 1000:1000 /tmp/"$dist"
  # mksquashfs /tmp/"$dist" "$dist".layer -comp zstd -Xcompression-level 15
  ./bin/mkdwarfs -i /tmp/"$dist" -o "$dist".layer

  # Create elf
  _create_elf "$dist".layer "$dist".flatimage

  # Create sha256sum
  sha256sum "$dist.flatimage" > dist/"$dist.flatimage.sha256sum"
  mv "$dist.flatimage" dist/
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
  cp "$FIM_DIR/sources/arch.list" arch/etc/pacman.d/mirrorlist

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
    docker build . --build-arg FIM_DIST=ARCH --build-arg FIM_DIR="$(pwd)" -t flatimage-boot -f docker/Dockerfile.boot
    docker run --rm -v "$FIM_DIR_BUILD":"/host" flatimage-boot cp "$FIM_DIR"/src/boot/build/Release/boot /host/bin
    docker run --rm -v "$FIM_DIR_BUILD":"/host" flatimage-boot cp "$FIM_DIR"/src/boot/janitor /host/bin
  )

  # Compile and include portal
  (
    cd "$FIM_DIR"
    docker build . -t "flatimage-portal" -f docker/Dockerfile.portal
    docker run --rm -v "$FIM_DIR_BUILD":/host "flatimage-portal" cp /fim/dist/fim_portal /fim/dist/fim_portal_daemon /host/bin
  )

  # Compile and include bwrap-apparmor
  (
    cd "$FIM_DIR"
    docker build . -t flatimage-bwrap-apparmor -f docker/Dockerfile.bwrap_apparmor
    docker run --rm -v "$FIM_DIR_BUILD":/host "flatimage-bwrap-apparmor" cp /fim/dist/fim_bwrap_apparmor /host/bin
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

  # MIME
  mkdir -p ./arch/fim/desktop
  cp "$FIM_DIR"/mime/icon.svg      ./arch/fim/desktop
  cp "$FIM_DIR"/mime/flatimage.xml ./arch/fim/desktop

  # Create layer 0 compressed filesystem
  chown -R 1000:1000 ./arch
  # chmod 777 -R ./arch
  # mksquashfs ./arch ./arch.layer -comp zstd -Xcompression-level 15
  ./bin/mkdwarfs -i ./arch -o ./arch.layer

  # Create elf
  _create_elf ./arch.layer ./arch.flatimage

  # Create sha256sum
  sha256sum arch.flatimage > dist/"arch.flatimage.sha256sum"
  mv ./"arch.flatimage" dist/
}

function main()
{
  rm -rf "$FIM_DIR_BUILD"
  mkdir "$FIM_DIR_BUILD"
  cd "$FIM_DIR_BUILD"

  case "$1" in
    "arch") _create_subsystem_arch ;;
    "alpine") _create_subsystem_alpine ;;
    "blueprint") _create_subsystem_blueprint ;;
    *) _die "Invalid option $2" ;;
  esac
}

main "$@"
