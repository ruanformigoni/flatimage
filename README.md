# Table of contents

- [Table of contents](#table-of-contents)
    - [What is FlatImage?](#what-is-flatimage)
    - [Comparison](#comparison)
- [Get FlatImage](#get-flatimage)
- [Usage](#usage)
    - [Options](#options)
    - [Environment Variables](#environment-variables)
        - [Configurable](#configurable)
        - [Read-Only](#read-only)
    - [Configure](#configure)
        - [HOME directory](#home-directory)
        - [Backend](#backend)
        - [Desktop Integration](#desktop-integration)
        - [Overlayfs](#overlayfs)
        - [Hooks](#hooks)
- [Use cases](#use-cases)
    - [Use pacman packages on non-arch systems](#use-pacman-packages-on-non-arch-systems)
    - [Use apt packages in non-debian systems](#use-apt-packages-in-non-debian-systems)
    - [Use alpine (apk) packages](#use-alpine-apk-packages)
    - [Use a pip package without installing pip](#use-a-pip-package-without-installing-pip)
    - [Use an npm package without installing npm](#use-an-npm-package-without-installing-npm)
        - [Outside the container](#outside-the-container)
        - [Inside the container](#inside-the-container)
    - [Compile an application without installing dependencies on the host](#compile-an-application-without-installing-dependencies-on-the-host)
        - [Outside the container](#outside-the-container)
        - [Inside the container](#inside-the-container)
- [Samples](#samples)
- [Usage Inside Docker](#usage-inside-docker)
- [Further Considerations](#further-considerations)
- [Motivations](#motivations)
- [Related Projects](#related-projects)

## What is FlatImage?

FlatImage, is a hybrid of [Flatpak](https://github.com/flatpak/flatpak)
sandboxing with [AppImage](https://github.com/AppImage/AppImageKit) portability.

FlatImage use case is twofold:

* A tool to package software that aims to work across several linux distros,
it bundles all the software dependencies and the software itself, within an
executable; unlike `AppImage`, FlatImage runs the application in a container, which
increases portability and compatibility at the cost of file size.

* A portable container image that requires no superuser permissions to run.

The diverse `GNU/Linux` ecosystem includes a vast array of distributions, each
with its own advantages and use cases. This can lead to cross-distribution
software compatibility challenges. FlatImage addresses these issues by:

* Utilizing its own root directory, enabling dynamic libraries with hard-coded
    paths to be packaged alongside the software without
    [binary patching](https://github.com/AppImage/AppImageKit/wiki/Bundling-Windows-applications).
* Running the application in its own gnu system, therefore, not using host
    libraries that might be outdated/incompatible with the application.

## Comparison

| Feature                                                                   | FlatImage     | Docker                     | AppImage |
| :---                                                                      | :---:         | :---:                      | :---:    |
| No superuser privileges to use                                            | x             | x<sup>2</sup>              | x
| **Overlayfs** (allows to install programs even after compression)             | x             |                            |
| No installation necessary (click and use)                                 | x             | Requires docker on the host| x
| Requires building on an old system to minimize glibc issues               |               | N/A                        | x
| Mountable as a filesystem                                                 | x             | x                          | x<sup>3</sup>
| Runs without mounting the filesystem                                      |               |                            | x
| Straightforward build process                                             | x             | x                          |
| Desktop integration                                                       | x             |                            | x
| Extract the contents                                                      | x             | x                          | x
| Supports reconfiguration without rebuild                                  | x             | x (layers)                 |
| No host libraries used (Filesystem Isolation)                             | x             | x                          |
| Supports compression of specific directories/files in the package         | x             |                            |
| Portable mutable user configuration                                       | x             | x                          |
| Granular control over containerization                                    | x             | x                          |
| Works without fuse installed (still requires kernel support)              | x<sup>4</sup> | x                          | x<sup>5</sup>
| Layered filesystem                                                        | x (overlayfs) | x                          |
| Advanced networking management                                            |               | x                          |
| Advanced security features                                                |               | x                          |

> 1. Requires superuser privileges
> 1. Only if the user is part of the docker group
> 1. Only as read-only, you can mount FIM as read-write, before compression.
> 1. Works without libfuse/libfuse3, still requires fusermount to be available.
> 1. Experimental implementations, available [here](https://github.com/probonopd/go-appimage) and [here](https://github.com/AppImage/type2-runtime)


# Get FlatImage

You can get the latest release [here](https://github.com/ruanformigoni/flatimage/releases), and extract the compressed archive with your file manager or with `tar -xf some-file.tar.xz`. To verify the image integrity, you can use the `sha256sum` file that accompanies each release, like so: `sha256sum -c some-file.flatimage.sha256sum`.

# Usage

You can enter the container simply by executing the downloaded image, e.g.,
`./arch.flatimage`, which should give you a prompt like this `(flatimage@arch) →`.
Remember to resize the image as shown in the examples before installing programs
on it, else there won't be enough space for it.

To enter the container as root (to install software) use:
```
./arch.flatimage fim-root bash
```

## Options

```
Avaliable options:
- fim-compress: Compress the filesystem to a read-only format.
- fim-root: Execute an arbitrary command as root.
- fim-exec: Execute an arbitrary command.
- fim-cmd: Set the default command to execute when no argument is passed.
- fim-resize: Resizes the filesystem.
    - # Resizes the filesytem to have 1G of size
    - E.g.: ./arch.flatimage fim-resize 1G
    - # Resizes the filesystem by current size plus 1G
    - E.g.: ./arch.flatimage fim-resize +1G
- fim-mount: Mount the filesystem in a specified directory
    - E.g.: ./arch.flatimage fim-mount ./mountpoint
- fim-xdg: Same as the 'fim-mount' command, however it opens the
    mount directory with xdg-open
- fim-perms-set: Set the permission for the container, available options are:
    pulseaudio, wayland, x11, session_bus, system_bus, gpu, input, usb
    - E.g.: ./arch.flatimage fim-perms pulseaudio,wayland,x11
- fim-perms-list: List the current permissions for the container
- fim-config-set: Sets a configuration that persists inside the image
    - E.g.: ./arch.flatimage fim-config-set home '"$FIM_FILE_BINARY".home
    - E.g.: ./arch.flatimage fim-config-set backend "proot"
- fim-config-list: List the current configurations for the container
    - E.g.: ./arch.flatimage fim-config-list                      # List all
    - E.g.: ./arch.flatimage fim-config-list "^dwarfs.*"          # List ones that match regex
- fim-dwarfs-add: Includes a dwarfs file inside the image, it is
                      automatically mounted on startup to the specified mount point
    - E.g.: ./arch.flatimage fim-dwarfs-add ./image.dwarfs /opt/image
- fim-dwarfs-list: Lists the dwarfs filesystems in the flatimage
    - E.g.: ./arch.flatimage fim-dwarfs-list
- fim-dwarfs-overlayfs: Makes dwarfs filesystems writteable again with overlayfs
    - E.g.: ./arch.flatimage fim-dwarfs-overlayfs usr '"$FIM_FILE_BINARY".config/overlays/usr'
- fim-hook-add-pre: Includes a hook that runs before the main command
    - E.g.: ./arch.flatimage fim-hook-add-pre ./my-hook
- fim-hook-add-post: Includes a hook that runs after the main command
    - E.g.: ./arch.flatimage fim-hook-add-post ./my-hook
- fim-help: Print this message.
```

## Environment Variables

### Configurable

* `FIM_NAME`: Name of the package, useful for desktop integration (default is flatimage-$FIM_DIST)
* `FIM_ICON`: Path to the application icon, default is '"$FIM_DIR_MOUNT"/fim/desktop/icon.svg'
* `FIM_BACKEND`: Back-end to use, default is `bwrap`, `proot` is also supported.
* `FIM_COMPRESSION_LEVEL`: Compression level of dwarfs (0-9), default is 6
* `FIM_SLACK_MINIMUM`: Free space always available on the filesystem startup,
    automatically grows it gets past it in the previous execution, default is
    50000000 bytes (50MB).
* `FIM_COMPRESSION_DIRS`: Directories to compress with dwarfs, default is `/usr /opt`.
* `FIM_DEBUG`: If defined to 1, print debug messages.

### Read-Only

* `FIM_DIST`: The linux distribution name (alpine, arch, ubuntu)
* `FIM_FILE_BINARY`: Full path to the executed binary file
* `FIM_BASENAME_BINARY`: Basename of the executed binary file.
* `FIM_DIR_BINARY`: The path to the directory of the executed binary file.
* `FIM_DIR_TEMP`: Location of the temporary runtime directory.
* `FIM_DIR_MOUNT`: Location of the runtime fim mountpoint.
* `FIM_MAIN_OFFSET`: Shows filesystem offset and exits.


The default path of `FIM` temporary files is `/tmp/fim`.

## Configure

### HOME directory

It is possible to change the default home path with `fim-config-set home`,
e.g.:

```bash
 # Default, uses the host's home directory
$ ./arch.flatimage fim-config-set home '$HOME'
 # The name of the flatimage binary plus an appended ".home"
$ ./arch.flatimage fim-config-set home '"$FIM_FILE_BINARY".home'
```

### Backend

Besides the environment variable `FIM_BACKEND`, it is possible to use the
`fim-config-set backend` to set the default tool. The environment variable
always takes precedence if defined.

```bash
 # Uses bwrap as the backend (default)
$ ./arch.flatimage fim-config-set backend "bwrap"
 # Uses proot as the backend
$ ./arch.flatimage fim-config-set backend "proot"
 # Run command on host using a static bash binary
$ ./arch.flatimage fim-config-set backend "host"
```

### Desktop Integration

Flatimage supports desktop integration, the icon and menu entry should appear
after the first execution. Make sure to have this line in your `~/.bashrc`
file:

```bash
export XDG_DATA_DIRS=$HOME/.local/share:$XDG_DATA_DIRS
```

After including it, log out and log in again (or reboot) for the changes apply.
Here's how to configure flatimage desktop integration:

```bash
 # Set the name of your application
$ ./arch.flatimage fim-config-set name "MyApp"
 # Set the icon, supported image types are '.svg,.png,.jpg'
$ ./arch.flatimage fim-config-set icon '"$FIM_DIR_MOUNT"/fim/desktop/icon.png'
 # Copy icon to inside the container
$ ./arch.flatimage fim-root cp ./my-image.png /fim/desktop/icon.png
 # Set the categories of your application
 # Avaliable categories found here: https://specifications.freedesktop.org/menu-spec/latest/apa.html
$ ./arch.flatimage fim-config-set categories "Game"
 # Enable desktop integration
$ ./arch.flatimage fim-config-set desktop 1
```

You can also disable desktop integration with:

```bash
 # Disable desktop integration
$ ./arch.flatimage fim-config-set desktop 0
```

To erase all desktop entries and icons created by flatimage, you can use the
command:

```bash
$ find ~/.local/share -iname "*flatimage*" -exec rm -v "{}" \;
```

### Overlayfs

You can use overlayfs on top of the `dwarfs` filesystems, e.g.:

```bash
 # List dwarfs filesystems
$ ./arch.flatimage fim-dwarfs-list
usr
opt
 # Suppose you want to make "usr" writteable again, use
$ ./arch.flatimage fim-dwarfs-overlayfs usr '"$FIM_FILE_BINARY".config/overlays/usr'
```

### Hooks

You can include `bash` hooks to execute code before and after your program:

```bash
 # Create hook dir
$ ./arch.flatimage fim-exec mkdir -p /fim/hooks/pre
 # Include hook
$ ./arch.flatimage fim-exec cp ./my-pre-hook /fim/hooks/pre
```

This is an example of a pre-hook file:
```bash
export MY_VAR="hello there, I'm defined inside flatimage!"

echo "Starting my cool program"

if [[ -v MY_OPTION ]]; then
  echo "You passed the option: ${MY_OPTION}!" 
fi
```

To remove the hook, just use:
```bash
./arch.flatimage fim-exec rm /fim/hooks/pre/my-pre-hook
```
... or enter the container, `cd /fim/hooks/pre` and erase the files.

To insert a post hook, just use the directory `post` instead.
```bash
 # Create hook dir
$ ./arch.flatimage fim-exec mkdir -p /fim/hooks/post
 # Include hook
$ ./arch.flatimage fim-exec cp ./my-post-hook /fim/hooks/post
```

# Use cases

To use FlatImage there is no need to install anything, simply download an image in
the [releases](https://gitlab.com/formigoni/art/-/releases) page, i.e., `focal
(ubuntu)`, `alpine` or `arch`. The archive is compressed, extract it and use it
as shown in the following examples.

## Use pacman packages on non-arch systems

```sh
 # Set the maximum filesystem size (use du -sh ./arch.flatimage to see actual size)
$ ./arch.flatimage fim-resize 4G
 # Set the desired permissions (you can also list them with fim-perms-list)
$ ./arch.flatimage fim-perms-set wayland,pulseaudio,gpu,session_bus
 # Install the desired application in the ubuntu subsystem
$ ./arch.flatimage fim-root fakechroot pacman -S firefox --noconfirm
 # Test the application
$ ./arch.flatimage fim-exec firefox
 # Set the default startup command
$ ./arch.flatimage fim-cmd firefox
 # (optional) Compress the package filesystem
$ ./arch.flatimage fim-compress
 # (optional) Rename the binary to the main application name
mv arch.flatimage firefox
 # Run the application (you can also click on it in your file manager)
$ ./firefox
```

## Use apt packages in non-debian systems

```sh
 # Set the maximum filesystem size (use du -sh ./focal.flatimage to see actual size)
$ ./focal.flatimage fim-resize 4G
 # Set the desired permissions (you can also list them with fim-perms-list)
$ ./focal.flatimage fim-perms-set wayland,pulseaudio,gpu,session_bus
 # Install the desired application in the ubuntu subsystem
$ ./focal.flatimage fim-root apt install -y firefox
 # Test the application
$ ./focal.flatimage fim-exec firefox
 # Set the default startup command
$ ./focal.flatimage fim-cmd firefox
 # (optional) Compress the package filesystem
$ ./focal.flatimage fim-compress
 # (optional) Rename the binary to the main application name
$ mv focal.flatimage firefox
 # Run the application (you can also click on it in your file manager)
$ ./firefox
```

## Use alpine (apk) packages

```sh
 # Set the maximum filesystem size (use du -sh ./alpine.flatimage to see actual size)
$ ./alpine.flatimage fim-resize 2G
 # Set the desired permissions (you can also list them with fim-perms-list)
$ ./alpine.flatimage fim-perms-set wayland,pulseaudio,gpu,session_bus
 # Install firefox with apk
$ ./alpine.flatimage fim-root apk add firefox font-noto
 # Test the application
$ ./alpine.flatimage fim-exec firefox
 # Set the default startup command
$ ./alpine.flatimage fim-cmd firefox
 # (optional) Compress the package filesystem
$ ./alpine.flatimage fim-compress
 # (optional) Rename the binary to the main application name
$ mv alpine.flatimage firefox
 # Run the application (you can also click on it in your file manager)
$ ./firefox
```

## Use a pip package without installing pip

__Archlinux__

```sh
 # Set the maximum filesystem size (use du -sh ./arch.flatimage to see actual size)
$ ./arch.flatimage fim-resize 4G
 # Install python-pipx
$ ./arch.flatimage fim-root fakechroot pacman -S python-pipx ffmpeg
 # Install the pip application inside the image
 export PIPX_HOME=/opt/pipx
 export PIPX_BIN_DIR=/usr/local/bin
$ ./arch.flatimage fim-root fakechroot pipx install yt-dlp
 # Test the application
$ ./arch.flatimage fim-exec yt-dlp -f "bestvideo+bestaudio" "https://www.youtube.com/watch?v=srnyVw-OR0g"
 # Set the default startup command
$ ./arch.flatimage fim-cmd yt-dlp
 # (optional) Compress the package filesystem
$ ./arch.flatimage fim-compress
 # (optional) Rename the binary to the main application name
$ mv arch.flatimage yt-dlp
 # Use the application (download youtube video)
$ ./yt-dlp -f 'bestvideo+bestaudio' 'https://www.youtube.com/watch?v=srnyVw-OR0g'
```

__Ubuntu__

```sh
 # Set the maximum filesystem size (use du -sh ./focal.flatimage to see actual size)
$ ./focal.flatimage fim-resize 4G
 # Install python-pip
$ ./focal.flatimage fim-root apt install -y python3-pip ffmpeg
 # Install the pip application inside the image
$ ./focal.flatimage fim-root pip3 install yt-dlp
 # Test the application
$ ./focal.flatimage fim-exec yt-dlp -f "bestvideo+bestaudio" https://www.youtube.com/watch?v=srnyVw-OR0g
 # Set the default startup command
$ ./focal.flatimage fim-cmd yt-dlp
 # (optional) Compress the package filesystem
$ ./focal.flatimage fim-compress
 # (optional) Rename the binary to the main application name
$ mv focal.flatimage yt-dlp.flatimage
 # Use the application (download youtube video)
$ ./yt-dlp.flatimage -f 'bestvideo+bestaudio' https://www.youtube.com/watch?v=srnyVw-OR0g
```

## Use an npm package without installing npm

### Outside the container

__Archlinux__

```sh
 # Set the maximum filesystem size (use du -sh ./arch.flatimage to see actual size)
$ ./arch.flatimage fim-resize 4G
 # Set the desired permissions (so vlc can play from inside the container)
$ ./arch.flatimage fim-perms-set x11,pulseaudio
 # Install npm/nodejs into the image
$ ./arch.flatimage fim-root fakechroot pacman -S nodejs npm vlc
 # Configure prefix to the root of the image
$ ./arch.flatimage fim-exec npm config set prefix -g '/usr/local/'
 # Install the npm application inside the image
$ ./arch.flatimage fim-root fakechroot npm install -g webtorrent-cli
 # Test the application
$ ./arch.flatimage fim-exec webtorrent "magnet:?xt=urn:btih:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c" --vlc
 # Set the default startup command
$ ./arch.flatimage fim-cmd webtorrent
 # (optional) Compress the package filesystem
$ ./arch.flatimage fim-compress
 # (optional) Rename the binary to the main application name
$ mv arch.flatimage webtorrent
 # Use the application (stream legal torrent video)
$ ./webtorrent "magnet:?xt=urn:btih:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c" --vlc
```

__Ubuntu__

```sh
 # Set the maximum filesystem size (use du -sh ./focal.flatimage to see actual size)
$ ./focal.flatimage fim-resize 4G
 # Set the desired permissions (so vlc can play from inside the container)
$ ./focal.flatimage fim-perms-set x11,pulseaudio
 # Install npm/nodejs into the image
$ ./focal.flatimage fim-root apt install -y curl
$ ./focal.flatimage fim-root curl -fsSL https://deb.nodesource.com/setup_19.x | bash -
$ ./focal.flatimage fim-root apt-get install -y nodejs mpv
 # Configure prefix to the root of the image
$ ./focal.flatimage fim-root npm config set prefix -g /usr/local
 # Install the npm application inside the image
$ ./focal.flatimage fim-root npm install -g webtorrent-cli
 # Test the application
$ ./focal.flatimage fim-exec webtorrent magnet:?xt=urn:btih:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c --mpv
 # Set the default startup command
$ ./focal.flatimage fim-cmd webtorrent
 # (optional) Compress the package filesystem
$ ./focal.flatimage fim-compress
 # (optional) Rename the binary to the main application name
$ mv focal.flatimage webtorrent.flatimage
 # Use the application (stream legal torrent video)
$ ./webtorrent.flatimage magnet:?xt=urn:btih:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c --mpv
```

Note that vlc/mpv was installed inside the image, it is required since the image
has no access to the host filesystem/applications.

### Inside the container

```sh
 # Set the maximum filesystem size (use du -sh ./arch.flatimage to see actual size)
$ ./arch.flatimage fim-resize 4G
 # Set the desired permissions (so vlc can play from inside the container)
$ ./arch.flatimage fim-perms-set x11,pulseaudio
 # Enter the container as root
$ FIM_ROOT=1 ./arch.flatimage
 # Install npm/nodejs into the image
(fim@arch) → fakechroot pacman -S nodejs npm vlc
 # Configure prefix to the root of the image
(fim@arch) → npm config set prefix -g '/usr/local/'
 # Install the npm application inside the image
(fim@arch) → fakechroot npm install -g webtorrent-cli
 # Press CTRL+D or type exit to quit the container
 # Enter the container as an unprivileged user
$ ./arch.flatimage
 # Test the application
(fim@arch) → webtorrent "magnet:?xt=urn:btih:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c" --vlc
 # Press CTRL+D or type exit to quit the container
 # Set the default startup command
$ ./arch.flatimage fim-cmd webtorrent
 # (optional) Compress the package filesystem
$ ./arch.flatimage fim-compress
 # (optional) Rename the binary to the main application name
$ mv arch.flatimage webtorrent
 # Use the application (stream legal torrent video)
$ ./webtorrent "magnet:?xt=urn:btih:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c" --vlc
```

## Compile an application without installing dependencies on the host

### Outside the container

```sh
 # Set the maximum filesystem size (use du -sh ./arch.flatimage to see actual size)
$ ./arch.flatimage fim-resize 4G
 # Fetch the application
$ git clone https://github.com/htop-dev/htop.git
 # Install the required build dependencies
$ ./arch.flatimage fim-root fakechroot pacman -S ncurses automake autoconf gcc make
 # Compile the application
cd htop
$ ../arch.flatimage fim-exec './autogen.sh'
$ ../arch.flatimage fim-exec './configure'
$ ../arch.flatimage fim-exec 'make'
 # Run the compiled application
$ ./htop
```

### Inside the container

```bash
 # Set the maximum filesystem size (use du -sh ./arch.flatimage to see actual size)
$ ./arch.flatimage fim-resize 4G
 # Fetch the application
$ git clone https://github.com/htop-dev/htop.git
 # Enter the container as root
$ FIM_ROOT=1 ./arch.flatimage
 # Install the required build dependencies
 (fim@arch) → fakechroot pacman -S ncurses automake autoconf gcc make
 # Compile
 (fim@arch) → cd htop
 (fim@arch) → ./autogen.sh
 (fim@arch) → ./configure
 (fim@arch) → make
 # Run
 (fim@arch) → ./htop
```

In this case `arch.flatimage` is now a portable building environment for htop.

# Samples

[1.](https://gitlab.com/formigoni/flatimage/-/blob/master/samples/sample-steam.sh?ref_type=heads) Portable steam

[2.](https://gitlab.com/formigoni/wine/-/releases) Portable wine

# Usage Inside Docker

Currently to run a flatimage inside docker, the `--privileged` flag is required.
Assuming that the current directory contains the image `alpine.flatimage`:

```
docker run --privileged -it --rm -v "$(pwd):/workdir" ubuntu:focal /bin/bash
cd workdir
./alpine.flatimage
```

# Further Considerations

FlatImage offers on build simplicity, packaging applications should be as simple as
installing them natively on the host system. This is an effort for the end-user
to not depend on the application developer to provide the portable binary (or to
handle how to package the application, dependencies and create a runner script).
It also simplifies the quality of life of the package developer, simplifying
the packaging process of applications.

# Motivations

1. The idea of this application sprung with the challenge to package software
   and dynamic libraries, such as `wine`, when there are hard-coded paths. The
   best solution is invasive
   [https://github.com/AppImage/AppImageKit/wiki/Bundling-Windows-applications](https://github.com/AppImage/AppImageKit/wiki/Bundling-Windows-applications)
   , which patches the binaries of wine directly to use a custom path for the
   32-bit libraries (an implementation of this concept is available
   [here](https://github.com/ruanformigoni/wine)), not only that, it requires to
   patch the `32-bit` pre-loader `ld-linux.so` as well, however, sometimes it
   still fails to execute properly. This is an over exceeding complexity for the
   end-user, which should package applications with no effort; `FlatImage`
   changes the root filesystem the application runs in, to a minimal gnu
   subsystem, and with that, it solves the previous issues with dynamic
   libraries no workarounds required. No host libraries are used, which
   decreases issues of portable applications working on one machine and not in
   other.

1. The fragmentation of the linux package management is considerable in modern
   times, e.g., `apt`, `pip`, `npm`, and more. To mitigate this issue
   `FlatImage` can perform the installation through the preferred package
   manager, and turn the program into an executable file, that can run in any
   linux distribution. E.g.: The user of `FlatImage` can create a binary of
   `youtube-dl`, from the `pip` package manager, without having either pip or
   python installed on the host operating system.

1. Some applications are offered as pre-compiled compressed tar files
   (tarballs), which sometimes only work when installed on the root of the
   operating system. However, doing so could hinder the operating system
   integrity, to avoid this issue `FlatImage` can install tarballs into itself
   and turn them into a portable binary.


# Related Projects

- [https://github.com/Kron4ek/Conty](https://github.com/Kron4ek/Conty)
- [https://github.com/genuinetools/binctr](https://github.com/genuinetools/binctr)
- [https://github.com/Intoli/exodus](https://github.com/Intoli/exodus)
- [https://statifier.sourceforge.net/](https://statifier.sourceforge.net/)
- [https://github.com/matthewbauer/nix-bundle](https://github.com/matthewbauer/nix-bundle)
- [https://github.com/containers/bubblewrap](https://github.com/containers/bubblewrap)
- [https://github.com/proot-me/proot](https://github.com/proot-me/proot)

<!-- // cmd: !./doc/toc.sh
