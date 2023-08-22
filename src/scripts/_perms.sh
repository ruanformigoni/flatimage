#!/usr/bin/env bash

######################################################################
# @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
# @file        : _perms
# @created     : Monday Jan 23, 2023 21:18:26 -03
#
# @description : Configure sandbox permissions
######################################################################

#shellcheck disable=2034

ARTS_PERM_PULSEAUDIO="${ARTS_PERM_PULSEAUDIO:-1}"
ARTS_PERM_WAYLAND="${ARTS_PERM_WAYLAND:-1}"
ARTS_PERM_X11="${ARTS_PERM_X11:-1}"
ARTS_PERM_SESSION_BUS="${ARTS_PERM_SESSION_BUS:-1}"
ARTS_PERM_SYSTEM_BUS="${ARTS_PERM_SYSTEM_BUS:-1}"
ARTS_PERM_GPU="${ARTS_PERM_GPU:-1}"
