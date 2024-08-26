#!/usr/bin/env bash

######################################################################
# @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
# @file        : dependencies
######################################################################

set -e

echo "digraph G {"

for i; do
  name_file="$i"
  name_base="${name_file%%.*}"

  for j in $(pcregrep -o1 "(ns_[a-zA-z0-9]+)" "$name_file" | grep -v "ns_$name_base" | sort -u); do
    echo "ns_$name_base -> $j"
  done
done

echo "}"
