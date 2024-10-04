set -e

# Currently defined bwrap binary
PATH_FILE_BWRAP_SOURCE="$1"
# Target to copy bwrap binary to
PATH_FILE_BWRAP_APPARMOR="$2"

# Find apparmor_parser
if ! PATH_FILE_APPARMOR_PARSER="$(command -v apparmor_parser)"; then
  echo "Could not find 'apparmor_parser' binary"
  exit 1
fi

# Copy bwrap binary
mkdir -pv "$(dirname "$PATH_FILE_BWRAP_APPARMOR")"
cp "$PATH_FILE_BWRAP_SOURCE" "$PATH_FILE_BWRAP_APPARMOR"
chmod 755 "$PATH_FILE_BWRAP_APPARMOR"

# Create profile
tee /etc/apparmor.d/bwrap <<END
abi <abi/4.0>,
include <tunables/global>

profile bwrap $PATH_FILE_BWRAP_APPARMOR flags=(unconfined) {
  userns,
}
END

# Reload profile
"$PATH_FILE_APPARMOR_PARSER" -r /etc/apparmor.d/bwrap

# vim: set ts=2 sw=2 nu rnu et:
