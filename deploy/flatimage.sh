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
