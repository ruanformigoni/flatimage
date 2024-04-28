#!/usr/bin/env bash

######################################################################
# @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
# @file        : flatimage
######################################################################

set -e

function fetch()
{
  # Fetch coreutils
  wget "https://github.com/ruanformigoni/coreutils-static/releases/download/d7f4cd2/coreutils-x86_64.tar.xz"

  # Extracts a bin/... folder
  tar xf "coreutils-x86_64.tar.xz"

  # Fetch Magick
  wget -O bin/magick "https://github.com/ruanformigoni/imagemagick-static-musl/releases/download/cc3f21c/magick-x86_64"
  wget -O bin/magick-license "https://raw.githubusercontent.com/ImageMagick/ImageMagick/main/LICENSE"
  chmod +x bin/magick

  # Fetch lsof
  wget -O./bin/lsof "https://github.com/ruanformigoni/lsof-static-musl/releases/download/e1a88fb/lsof-x86_64"

  # Fetch e2fsprogs
  wget -O ./bin/fuse2fs   "https://github.com/ruanformigoni/e2fsprogs/releases/download/e58f946/fuse2fs"
  wget -O ./bin/mke2fs    "https://github.com/ruanformigoni/e2fsprogs/releases/download/e58f946/mke2fs"
  wget -O ./bin/e2fsck    "https://github.com/ruanformigoni/e2fsprogs/releases/download/e58f946/e2fsck"
  wget -O ./bin/resize2fs "https://github.com/ruanformigoni/e2fsprogs/releases/download/e58f946/resize2fs"

  # Fetch proot
  wget -O ./bin/proot "https://github.com/ruanformigoni/proot/releases/download/5a4be11/proot"

  # Fetch bwrap
  wget -O ./bin/bwrap "https://github.com/ruanformigoni/bubblewrap-musl-static/releases/download/559a725/bwrap"

  # Fetch overlayfs
  wget -O ./bin/overlayfs "https://github.com/ruanformigoni/fuse-overlayfs/releases/download/af507a2/fuse-overlayfs-x86_64"

  # Fetch dwarfs
  wget -O bin/dwarfs_aio "https://github.com/mhx/dwarfs/releases/download/v0.9.8/dwarfs-universal-0.9.8-Linux-x86_64-clang"
  ln -sf dwarfs_aio bin/mkdwarfs
  ln -sf dwarfs_aio bin/dwarfsextract
  ln -sf dwarfs_aio bin/dwarfs

  # Fetch bash
  wget -O ./bin/bash "https://github.com/ruanformigoni/bash-static/releases/download/5355251/bash-linux-x86_64"

  # Setup xdg
  cp src/xdg/xdg-* bin
  chmod +x bin/xdg-*
}

function build()
{
  # Build portal
  docker build . -t "flatimage-portal:$1" -f docker/Dockerfile.portal
  docker run --rm -v "$(pwd)":/workdir "flatimage-portal:$1" cp /fim/dist/portal_guest /workdir/bin/fim_portal_guest
  docker run --rm -v "$(pwd)":/workdir "flatimage-portal:$1" cp /fim/dist/portal_host /workdir/bin/fim_portal_host

  # Build the main elf
  docker build . --build-arg FIM_DIST="$1" -t "flatimage:$1" -f docker/Dockerfile.elf
  docker run --rm -v "$(pwd)":/workdir "flatimage:$1" cp /fim/dist/main /workdir/elf-"$1"
}

function main()
{
  case "$1" in
    "fetch") fetch ;;
    "build") build "$2" ;;
  esac
}

main "$@"
