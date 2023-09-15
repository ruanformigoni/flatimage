#!/usr/bin/env bash

######################################################################
# @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
# @file        : _perms
# @created     : Monday Jan 23, 2023 21:18:26 -03
#
# @description : Configure sandbox permissions
######################################################################

#shellcheck disable=2034

FIM_PERM_PULSEAUDIO="${FIM_PERM_PULSEAUDIO:-0}"
FIM_PERM_WAYLAND="${FIM_PERM_WAYLAND:-0}"
FIM_PERM_X11="${FIM_PERM_X11:-0}"
FIM_PERM_SESSION_BUS="${FIM_PERM_SESSION_BUS:-0}"
FIM_PERM_SYSTEM_BUS="${FIM_PERM_SYSTEM_BUS:-0}"
FIM_PERM_GPU="${FIM_PERM_GPU:-0}"
