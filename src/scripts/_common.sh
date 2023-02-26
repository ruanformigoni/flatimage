#!/usr/bin/env bash

######################################################################
# @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
# @file        : _common
# @created     : Monday Jan 23, 2023 21:12:20 -03
#
# @description : Utilities
######################################################################

PID="$$"

# Emits a message in &2
# $(1..n-1) arguments to echo
# $n message
function _msg()
{
  echo -e "${@:1:${#@}-1}" "[\033[32m*\033[m] ${*: -1}" >&2;
}

# Quits the program
# $* = Termination message
function _die()
{
  [ "$*" ] && _msg "$*"
  kill -s SIGTERM "$PID"
  exit 1
}

# Checks dependencies
# $(1..n) dependencies
function _requires()
{
  for i; do
    if ! command -v "$i" &>/dev/null ; then
      _msg "Missing dependency $i"
      exit 1
    fi
  done
}

