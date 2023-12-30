#!/usr/bin/env bash

######################################################################
# @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
# @file        : analyze
# @created     : Saturday Dec 30, 2023 07:12:21 -03
######################################################################

DIR_SCRIPT=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

STREAM=/dev/null

declare -i size_mb_input=10

while [ "$size_mb_input" -lt 50000 ]; do
  # Display requested size
  echo -en "${size_mb_input}M\t"
  # Create file filesystem
  "$DIR_SCRIPT/truncate" -s "$size_mb_input"M out.fs &>"$STREAM"
  "$DIR_SCRIPT/mke2fs" -b1024 -t ext2 out.fs &>"$STREAM"
  # Mount
  mkdir -p "$DIR_SCRIPT"/mount
  "$DIR_SCRIPT/fuse2fs" out.fs mount &>"$STREAM"
  # Display actual size
  read -r size_mb_real < <(df -BM --output=avail /home/ruan/Repositories/flatimage/doc/mount \
    | tail -n1 \
    | xargs)
  echo "${size_mb_real}"
  # Cleanup
  fusermount -u "$DIR_SCRIPT"/mount
  rmdir mount
  rm out.fs
  # Increment
  size_mb_input=$((size_mb_input+50))
done
