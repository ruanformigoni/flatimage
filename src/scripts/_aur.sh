#!/usr/bin/env bash

######################################################################
# @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
# @file        : aur
# @created     : Wednesday Mar 01, 2023 18:17:55 -03
#
# @description : Install AUR packages
######################################################################

set -e

_die()
{
  echo "[*] $*" >&2; exit 1
}

[ -z "$1" ] && _die "No package specified for installation"
#shellcheck disable=2016
[ -z "$HOST_USERNAME" ] && _die 'Empty HOST_USERNAME variable, set it with "export HOST_USERNAME=$(whoami)"'

pkg="$1"

# Define root passwd
echo "root:arch" | chpasswd

# Add current user if not exists
id -u "$HOST_USERNAME" &>/dev/null || useradd "$HOST_USERNAME"

# Allow su execution as another user
chmod u+s /bin/su

# Install package
su - "$HOST_USERNAME" -c "yay -S $pkg"
