# Arts - Application Root Subsystem

# Table of contents

- [Arts - Application Root Subsystem](#arts---application-root-subsystem)
- [Table of contents](#table-of-contents)
  - [What is Arts?](#what-is-arts?)
  - [Motivations](#motivations)
  - [Background](#background)
- [Get Arts](#get-arts)
- [Usage](#usage)
  - [Options](#options)
  - [Environment Variables](#environment-variables)
- [Use cases](#use-cases)
  - [Package pre-compiled binaries](#package-pre-compiled-binaries)
  - [Use an apt package in non-debian systems](#use-an-apt-package-in-non-debian-systems)
  - [Use a pacman package on non-arch systems](#use-a-pacman-package-on-non-arch-systems)
  - [Use an AUR package on non-arch systems](#use-an-aur-package-on-non-arch-systems)
  - [Use a pip package without installing pip/python](#use-a-pip-package-without-installing-pippython)
  - [Use a npm package without installing npm/nodejs](#use-a-npm-package-without-installing-npmnodejs)
  - [Compile an application without installing dependencies on the host](#compile-an-application-without-installing-dependencies-on-the-host)
- [Todo](#todo)
- [Related Projects](#related-projects)

## What is Arts?

Application Root Subsystem (Arts), is a tool to package software into a portable
binary. It packs all the software dependencies, and the software itself, within
an executable that works across several linux distributions. The differences
between the available alternatives, such as `AppImage`, is that it uses a
`proot` based approach, i.e., it does not allow the software to use host
libraries by default. With that, it also solves issues with hard-coded binary
paths for dynamic libraries.

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


## Background

In today's `GNU/Linux` ecosystem there is a daunting number of distributions with
their own advantages and use cases, this raises an issue with cross-distro
software compatibility. Arts is a tool to mitigate this issue, it solves the
following issues:

* Arts extends on the one app = one file, giving the user the option to keep
    their configurations saved inside the application itself.
* Arts uses its own root directory, so dynamic libraries can be packaged together
    with the software without binary patching.
* From the previous bullet, arts generates packaged software that work in several
    linux distributions.
* Arts generates a binary that works without requiring installation, click
    and use.
* Arts does not require root permissions.
* The application only has access files/folders the user specifies.
* Updates to the host system libraries do not break arts application.
* Supports reconfiguration without rebuild
* No host libraries used (Filesystem Isolation)
* Supports selective directory compression
* Portable user configuration within the package

# Get Arts

You can get the latest release [here](https://gitlab.com/formigoni/arts/-/releases).

# Usage

## Options

```
Application Chroot Subsystem (Arts)
Avaliable options:
- arts-compress: Compress the filesystem to a read-only format.
- arts-tarball: Install a tarball in the container's '/'.
- arts-exec: Execute an arbitrary command.
- arts-root: Execute an arbitrary command as root.
- arts-cmd: Set the default command to execute.
- arts-resize: Resize the filesystem.
- arts-mount: Mount the filesystem in a specified directory
    - E.g.: ./focal.arts arts-mount ./mountpoint
- arts-xdg: Same as the 'arts-mount' command, however it opens the
    mount directory with xdg-open
- arts-help: Print this message.
```

## Environment Variables

* `ARTS_COMPRESSION_LEVEL`: Compression level of dwarfs (0-9), default is 6
* `ARTS_COMPRESSION_SLACK`: Extra space after filesystem is resized on
compression, default is 50000 (50MB).
* `ARTS_COMPRESSION_DIRS`: Directories to compress with dwarfs, default is `/usr /opt`.
* `ARTS_DEBUG`: If defined to 1, print debug messages.


# Use cases

To use arts there is no need to install anything, simply download an image in
the [releases](https://gitlab.com/formigoni/art/-/releases) page, i.e., `focal
(ubuntu)` or `arch`. The archive is compressed, extract it and use it as shown
in the following examples.

## Package pre-compiled binaries

For this example, we use the [neovim](https://github.com/neovim/neovim/releases)
`.tar.gz` package.

```sh
 # 1. Set the filesystem as RW
 export ARTS_RW=1
 # 2. Set the maximum filesystem size (use du -sh ./focal.arts to see actual size)
./focal.arts arts-resize 10G
 # 3. Install tarball in ubuntu root
./focal.arts arts-tarball nvim-linux64.tar.gz
 # 4. Test the program
./focal.arts arts-exec nvim
 # 5. Set the default startup command
./focal.arts arts-cmd nvim
 # 6. (optional) Compress the package filesystem
./focal.arts arts-compress
 # 7. (optional) Rename the binary to the main application name
mv focal.arts nvim.arts
 # 8. Run the application
./nvim.arts
```

## Use an apt package in non-debian systems

```sh
 # 1. Set the filesystem as RW
 export ARTS_RW=1
 # 2. Set the maximum filesystem size (use du -sh ./focal.arts to see actual size)
./focal.arts arts-resize 10G
 # 3. Install the desired application in the ubuntu subsystem
./focal.arts arts-root apt install -y firefox
 # 4. Test the application
./focal.arts arts-exec firefox
 # 5. Set the default startup command
./focal.arts arts-cmd firefox
 # 6. (optional) Compress the package filesystem
./focal.arts arts-compress
 # 7. (optional) Rename the binary to the main application name
mv focal.arts firefox.arts
 # 8. Run the application (you can also click on it in your file manager)
./firefox.arts
```

## Use a pacman package on non-arch systems

```sh
 # 1. Set the filesystem as RW
 export ARTS_RW=1
 # 2. Set the maximum filesystem size (use du -sh ./focal.arts to see actual size)
./arch.arts arts-resize 10G
 # 3. Install the desired application in the ubuntu subsystem
./arch.arts arts-root pacman -S firefox --noconfirm
 # 4. Test the application
./arch.arts arts-exec firefox
 # 5. Set the default startup command
./arch.arts arts-cmd firefox
 # 6. (optional) Compress the package filesystem
./arch.arts arts-compress
 # 7. (optional) Rename the binary to the main application name
mv arch.arts firefox.arts
 # 8. Run the application (you can also click on it in your file manager)
./firefox.arts
```

## Use an AUR package on non-arch systems

```sh
 # 1. Set the filesystem as RW
 export ARTS_RW=1
 # 2. Set the maximum filesystem size (use du -sh ./focal.arts to see actual size)
./arch.arts arts-resize 10G
 # 3. Use the included aur script (root password is arch)
./arch.arts arts-root aur librewolf-bin
 # 4. Test the application
./arch.arts arts-exec librewolf
 # 5. Set the default startup command
./arch.arts arts-cmd librewolf
 # 6. (optional) Compress the package filesystem
./arch.arts arts-compress
 # 7. (optional) Rename the binary to the main application name
mv arch.arts librewolf.arts
 # 8. Run the application (you can also click on it in your file manager)
./librewolf.arts
```

## Use a pip package without installing pip/python

```sh
 # 1. Set the filesystem as RW
 export ARTS_RW=1
 # 2. Set the maximum filesystem size (use du -sh ./focal.arts to see actual size)
./focal.arts arts-resize 10G
 # 3. Install python-pip
./focal.arts arts-root apt install -y python3-pip
 # 4. Install the pip application inside the image
./focal.arts arts-root pip3 install youtube-dl
 # 5. Test the application
./focal.arts arts-exec youtube-dl -f 'bestvideo+bestaudio' https://www.youtube.com/watch?v=srnyVw-OR0g
 # 6. Set the default startup command
./focal.arts arts-cmd youtube-dl
 # 7. (optional) Compress the package filesystem
./focal.arts arts-compress
 # 8. (optional) Rename the binary to the main application name
mv focal.arts youtube-dl.arts
 # 9. Use the application (download youtube video)
./youtube-dl.arts -f 'bestvideo+bestaudio' https://www.youtube.com/watch?v=srnyVw-OR0g
```

## Use a npm package without installing npm/nodejs

```sh
 # 1. Set the filesystem as RW
 export ARTS_RW=1
 # 2. Set the maximum filesystem size (use du -sh ./focal.arts to see actual size)
./focal.arts arts-resize 10G
 # 3. Install npm/nodejs into the image
./focal.arts arts-root apt install -y curl
./focal.arts arts-root 'curl -fsSL https://deb.nodesource.com/setup_19.x \| bash -'
./focal.arts arts-root apt-get install -y nodejs mpv
 # 4. Install the npm application inside the image
./focal.arts arts-root npm install webtorrent-cli
 # 5. Test the application
./focal.arts arts-exec webtorrent magnet:?xt=urn:btih:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c --mpv
 # 6. Set the default startup command
./focal.arts arts-cmd webtorrent
 # 7. (optional) Compress the package filesystem
./focal.arts arts-compress
 # 8. (optional) Rename the binary to the main application name
mv focal.arts webtorrent.arts
 # 9. Use the application (stream legal torrent video)
./webtorrent.arts magnet:?xt=urn:btih:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c --mpv
```

Note that step 2 also installed mpv inside the image, it is required since the
image has no access to the host filesystem/applications.

## Compile an application without installing dependencies on the host

```sh
 # 1. Set the filesystem as RW
 export ARTS_RW=1
 # 2. Set the maximum filesystem size (use du -sh ./focal.arts to see actual size)
./focal.arts arts-resize 10G
 # 3. Fetch the application
git clone https://github.com/htop-dev/htop.git
 # 4. Install the required build dependencies
./focal.arts arts-root apt install -y libncursesw5-dev autotools-dev autoconf automake build-essential
 # 5. (optional) Compress the package filesystem
./focal.arts arts-compress
 # 6. Compile the application
cd htop
../focal.arts arts-exec './autogen.sh && ./configure && make'
 # 7. Run the compiled application
./htop
```

In this case `focal.arts` is now a portable building environment for htop.


# Todo

- [ ] Desktop Integration
- [ ] Application themes (as in linux mint)

# Related Projects

- [https://github.com/Kron4ek/Conty](https://github.com/Kron4ek/Conty)
- [https://github.com/genuinetools/binctr](https://github.com/genuinetools/binctr)
- [https://github.com/Intoli/exodus](https://github.com/Intoli/exodus)
- [https://statifier.sourceforge.net/](https://statifier.sourceforge.net/)
- [https://github.com/matthewbauer/nix-bundle](https://github.com/matthewbauer/nix-bundle)

<!-- // cmd: !./doc/toc.sh
