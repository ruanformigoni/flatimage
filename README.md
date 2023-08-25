# Arts - Application Root Subsystem

# Table of contents

- [Arts - Application Root Subsystem](#arts---application-root-subsystem)
- [Table of contents](#table-of-contents)
  - [What is Arts?](#what-is-arts?)
  - [Comparison](#comparison)
- [Get Arts](#get-arts)
- [Usage](#usage)
  - [Options](#options)
  - [Environment Variables](#environment-variables)
- [Use cases](#use-cases)
  - [Use apt packages in non-debian systems](#use-apt-packages-in-non-debian-systems)
  - [Use pacman packages on non-arch systems](#use-pacman-packages-on-non-arch-systems)
  - [Use alpine (apk) packages](#use-alpine-apk-packages)
  - [Use a pip package without installing pip/python](#use-a-pip-package-without-installing-pippython)
  - [Use a npm package without installing npm/nodejs](#use-a-npm-package-without-installing-npmnodejs)
  - [Compile an application without installing dependencies on the host](#compile-an-application-without-installing-dependencies-on-the-host)
  - [Further Considerations](#further-considerations)
  - [Motivations](#motivations)
- [Related Projects](#related-projects)

## What is Arts?

Application Root Subsystem (Arts), is the bastard child of
[Flatpak](https://github.com/flatpak/flatpak) and
[AppImage](https://github.com/AppImage/AppImageKit).

Arts use case is twofold:

* A tool to package software that aims to work across several linux distros,
it bundles all the software dependencies and the software itself, within an
executable; unlike `AppImage`, Arts runs the application in a container, which
increases portability and compatibility at the cost of file size.

* A portable container image that requires no superuser permissions to run.

The diverse `GNU/Linux` ecosystem includes a vast array of distributions, each
with its own advantages and use cases. This can lead to cross-distribution
software compatibility challenges. Arts addresses these issues by:

* Utilizing its own root directory, enabling dynamic libraries with hard-coded
    paths to be packaged alongside the software without
    [binary patching](https://github.com/AppImage/AppImageKit/wiki/Bundling-Windows-applications).
* Running the application in its own gnu system, therefore, not using host
    libraries that might be outdated/incompatible with the application.

## Comparison

| Feature                                                                   | Arts          | Docker                     | AppImage |
| :---                                                                      | :---:         | :---:                      | :---:    |
| No superuser privileges to use                                            | x             | x<sup>2</sup>              | x
| No installation necessary (click and use)                                 | x             | Requires docker on the host| x
| Mountable as a filesystem                                                 | x             | x                          | x<sup>3</sup>
| Runs without mounting the filesystem                                      | x<sup>1</sup> |                            | x
| Straightforward build process                                             | x             | x                          |
| Desktop integration                                                       |               |                            | x
| Extract the contents                                                      | x             | x                          | x
| Supports reconfiguration without rebuild                                  | x             | x (layers)                 |
| No host libraries used (Filesystem Isolation)                             | x             | x                          |
| Supports compression of specific directories/files in the package         | x             |                            |
| Portable mutable user configuration                                       | x             | x                          |
| Granular control over containerization                                    | x             | x                          |
| Works without fuse installed (still requires kernel support)              | x<sup>4</sup> | x                          | x<sup>5</sup>
| Layered filesystem                                                        |               | x                          |
| Advanced networking management                                            |               | x                          |
| Advanced security features                                                |               | x                          |

> 1. Requires superuser privileges
> 1. Only if the user is part of the docker group
> 1. Only as read-only, you can mount ARTS as read-write, before compression.
> 1. Works without libfuse/libfuse3, still requires fusermount to be available.
> 1. Experimental implementations, available [here](https://github.com/probonopd/go-appimage) and [here](https://github.com/AppImage/type2-runtime)


# Get Arts

You can get the latest release [here](https://gitlab.com/formigoni/arts/-/releases).

# Usage

## Options

```
Application Root Subsystem (Arts)
Avaliable options:
- arts-compress: Compress the filesystem to a read-only format.
- arts-root: Execute an arbitrary command as root.
- arts-exec: Execute an arbitrary command.
- arts-cmd: Set the default command to execute when no argument is passed.
- arts-resize: Resize the filesystem.
- arts-mount: Mount the filesystem in a specified directory
    - E.g.: ./focal.arts arts-mount ./mountpoint
- arts-xdg: Same as the 'arts-mount' command, however it opens the
    mount directory with xdg-open
- arts-perms: Set the permission for the container, available options are:
    pulseaudio, wayland, x11, session_bus, system_bus, gpu
    - E.g.: ./focal.arts arts-perms pulseaudio,wayland,x11
- arts-help: Print this message.
```

## Environment Variables

### Configurable

* `ARTS_BACKEND`: Back-end to use, default is `bwrap`, `proot` is also supported.
* `ARTS_COMPRESSION_LEVEL`: Compression level of dwarfs (0-9), default is 6
* `ARTS_COMPRESSION_SLACK`: Extra space after filesystem is resized on
compression, default is 50000 (50MB).
* `ARTS_COMPRESSION_DIRS`: Directories to compress with dwarfs, default is `/usr /opt`.
* `ARTS_DEBUG`: If defined to 1, print debug messages.

### Read-Only

* `ARTS_FILE_BINARY`: The path to the executed binary file.
* `ARTS_DIR_BINARY`: The path to the directory of the executed binary file.
* `ARTS_DIR_TEMP`: Location of the temporary runtime directory.
* `ARTS_DIR_MOUNT`: Location of the runtime arts mountpoint.
* `ARTS_MAIN_OFFSET`: Shows filesystem offset and exits.


The default path of `ARTS` temporary files is `/tmp/arts`.

## Configure

### HOME directory

It is possible to change the default home path with `arts-config-set home`,
e.g.:

```bash
# Default, uses the host's home directory
./arch.arts arts-config-set home '$HOME'
# Use the directory where the arts binary is located / arch.home, if called from
# $HOME/Downloads, the home directory would be $HOME/Downloads/arch.home.
./arch.arts arts-config-set home '"$ARTS_FILE_LOCATION"/arch.home'
# You can use subshells to compute the path, in this case it computes the
# directory where the arts binary is in, same as "$ARTS_FILE_LOCATION"
./arch.arts arts-config-set home '"$(dirname "$ARTS_FILE")"/arch.home'
```

### Backend

Besides the environment variable `ARTS_BACKEND`, it is possible to use the
`arts-config-set backend` to set the default tool. The environment variable
always takes precedence if defined.

```bash
# Uses bwrap as the backend (default)
./arch.arts arts-config-set backend "bwrap"
# Uses proot as the backend
./arch.arts arts-config-set backend "proot"
```

# Use cases

To use arts there is no need to install anything, simply download an image in
the [releases](https://gitlab.com/formigoni/art/-/releases) page, i.e., `focal
(ubuntu)`, `alpine` or `arch`. The archive is compressed, extract it and use it
as shown in the following examples.

## Use apt packages in non-debian systems

```sh
 # Set the maximum filesystem size (use du -sh ./focal.arts to see actual size)
./focal.arts arts-resize 4G
 # Install the desired application in the ubuntu subsystem
./focal.arts arts-root 'apt install -y firefox'
 # Set the desired permissions (you can also list them with arts-perms-list)
 ./focal.arts arts-perms-set wayland,pulseaudio,gpu,session_bus
 # Test the application
./focal.arts arts-exec firefox
 # Set the default startup command
./focal.arts arts-cmd firefox
 # (optional) Compress the package filesystem
./focal.arts arts-compress
 # (optional) Rename the binary to the main application name
mv focal.arts firefox.arts
 # Run the application (you can also click on it in your file manager)
./firefox.arts
```

## Use pacman packages on non-arch systems

```sh
 # Set the maximum filesystem size (use du -sh ./focal.arts to see actual size)
./arch.arts arts-resize 4G
 # Install the desired application in the ubuntu subsystem
./arch.arts arts-root 'pacman -S firefox --noconfirm'
 # Set the desired permissions (you can also list them with arts-perms-list)
./arch.arts arts-perms-set wayland,pulseaudio,gpu,session_bus
 # Test the application
./arch.arts arts-exec firefox
 # Set the default startup command
./arch.arts arts-cmd firefox
 # (optional) Compress the package filesystem
./arch.arts arts-compress
 # (optional) Rename the binary to the main application name
mv arch.arts firefox.arts
 # Run the application (you can also click on it in your file manager)
./firefox.arts
```

## Use alpine (apk) packages

```sh
 # Set the maximum filesystem size (use du -sh ./focal.arts to see actual size)
./alpine.arts arts-resize 2G
 # Install firefox with apk
./alpine.arts arts-root 'apk add firefox font-noto'
 # Set the desired permissions (you can also list them with arts-perms-list)
./alpine.arts arts-perms-set wayland,pulseaudio,gpu,session_bus
 # Test the application
./alpine.arts arts-exec firefox
 # Set the default startup command
./alpine.arts arts-cmd firefox
 # (optional) Compress the package filesystem
./alpine.arts arts-compress
 # (optional) Rename the binary to the main application name
mv alpine.arts firefox.arts
 # Run the application (you can also click on it in your file manager)
./firefox.arts
```

## Use a pip package without installing pip/python

### Example with focal

```sh
 # Set the maximum filesystem size (use du -sh ./focal.arts to see actual size)
./focal.arts arts-resize 4G
 # Install python-pip
./focal.arts arts-root 'apt install -y python3-pip ffmpeg'
 # Install the pip application inside the image
./focal.arts arts-root 'pip3 install yt-dlp'
 # Test the application
./focal.arts arts-exec 'yt-dlp -f "bestvideo+bestaudio" https://www.youtube.com/watch?v=srnyVw-OR0g'
 # Set the default startup command
./focal.arts arts-cmd yt-dlp
 # (optional) Compress the package filesystem
./focal.arts arts-compress
 # (optional) Rename the binary to the main application name
mv focal.arts yt-dlp.arts
 # Use the application (download youtube video)
./yt-dlp.arts -f 'bestvideo+bestaudio' https://www.youtube.com/watch?v=srnyVw-OR0g
```

### Example with arch

```sh
 # Set the maximum filesystem size (use du -sh ./arch.arts to see actual size)
./arch.arts arts-resize 4G
 # Install python-pip
 ./arch.arts arts-root 'pacman -S python-pipx ffmpeg'
 # Install the pip application inside the image
./arch.arts arts-root 'PIPX_HOME=/opt/pipx PIPX_BIN_DIR=/usr/local/bin pipx install yt-dlp'
 # Test the application
./arch.arts arts-exec 'yt-dlp -f "bestvideo+bestaudio" https://www.youtube.com/watch?v=srnyVw-OR0g'
 # Set the default startup command
./arch.arts arts-cmd yt-dlp
 # (optional) Compress the package filesystem
./arch.arts arts-compress
 # (optional) Rename the binary to the main application name
mv arch.arts yt-dlp.arts
 # Use the application (download youtube video)
./yt-dlp.arts -f 'bestvideo+bestaudio' https://www.youtube.com/watch?v=srnyVw-OR0g
```

## Use an npm package without installing npm/nodejs

Currently only works on the `proot` backend

```sh
 # Set the maximum filesystem size (use du -sh ./focal.arts to see actual size)
./focal.arts arts-resize 4G
 # Install npm/nodejs into the image
./focal.arts arts-root 'apt install -y curl'
./focal.arts arts-root 'curl -fsSL https://deb.nodesource.com/setup_19.x | bash -'
./focal.arts arts-root 'apt-get install -y nodejs mpv'
 # Install the npm application inside the image
./focal.arts arts-root 'npm config set prefix /usr/local'
./focal.arts arts-root 'npm install -g webtorrent-cli'
 # Test the application
./focal.arts arts-exec 'webtorrent magnet:?xt=urn:btih:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c --mpv'
 # Set the default startup command
./focal.arts arts-cmd webtorrent
 # (optional) Compress the package filesystem
./focal.arts arts-compress
 # (optional) Rename the binary to the main application name
mv focal.arts webtorrent.arts
 # Use the application (stream legal torrent video)
./webtorrent.arts magnet:?xt=urn:btih:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c --mpv
```

Note that step 2 also installed mpv inside the image, it is required since the
image has no access to the host filesystem/applications.

## Compile an application without installing dependencies on the host

```sh
 # Set the maximum filesystem size (use du -sh ./focal.arts to see actual size)
./focal.arts arts-resize 4G
 # Fetch the application
git clone https://github.com/htop-dev/htop.git
 # Install the required build dependencies
./focal.arts arts-root 'apt install -y libncursesw5-dev autotools-dev autoconf automake build-essential'
 # Compile the application
cd htop
../focal.arts arts-exec './autogen.sh && ./configure && make'
 # Run the compiled application
./htop
```

In this case `focal.arts` is now a portable building environment for htop.

## Further Considerations

Arts offers on build simplicity, packaging applications should be as simple as
installing them natively on the host system. This is an effort for the end-user
to not depend on the application developer to provide the portable binary (or to
handle how to package the application, dependencies and create a runner script).
It also simplifies the quality of life of the package developer, simplifying
the packaging process of applications.

## Motivations

1. The idea of this application sprung with the challenge to package software
   and dynamic libraries, such as `wine`, when there are hard-coded paths. The
   best solution is invasive
   [https://github.com/AppImage/AppImageKit/wiki/Bundling-Windows-applications](https://github.com/AppImage/AppImageKit/wiki/Bundling-Windows-applications)
   , which patches the binaries of wine directly to use a custom path for the
   32-bit libraries (an implementation of this concept is available
   [here](https://github.com/ruanformigoni/wine)), not only that, it requires to
   patch the `32-bit` pre-loader `ld-linux.so` as well, however, sometimes it
   still fails to execute properly. This is an over exceeding complexity for the
   end-user, which should package applications with no effort; `Arts` changes
   the root filesystem the application runs in, to a minimal gnu subsystem, and
   with that, it solves the previous issues with dynamic libraries no
   workarounds required. No host libraries are used, which decreases issues of
   portable applications working on one machine and not in other.

1. The fragmentation of the linux package management is considerable in modern
   times, e.g., `apt`, `pip`, `npm`, and more. To mitigate this issue `Arts` can
   perform the installation through the preferred package manager, and turn the
   program into an executable file, that can run in any linux distribution.
   E.g.: The user of `Arts` can create a binary of `youtube-dl`, from the `pip`
   package manager, without having either pip or python installed on the host
   operating system.

1. Some applications are offered as pre-compiled compressed tar files
   (tarballs), which sometimes only work when installed on the root of the
   operating system. However, doing so could hinder the operating system
   integrity, to avoid this issue `Arts` can install tarballs into itself and
   turn them into a portable binary.


# Related Projects

- [https://github.com/Kron4ek/Conty](https://github.com/Kron4ek/Conty)
- [https://github.com/genuinetools/binctr](https://github.com/genuinetools/binctr)
- [https://github.com/Intoli/exodus](https://github.com/Intoli/exodus)
- [https://statifier.sourceforge.net/](https://statifier.sourceforge.net/)
- [https://github.com/matthewbauer/nix-bundle](https://github.com/matthewbauer/nix-bundle)

<!-- // cmd: !./doc/toc.sh
