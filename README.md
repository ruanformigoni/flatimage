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
            - [Desktop Entry and File Manager Icon](#desktop-entry-and-file-manager-icon)
            - [XDG-Open](#xdg-open)
        - [Overlayfs](#overlayfs)
        - [Hooks](#hooks)
    - [Portals](#portals)
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
    - [Portable steam](#portable-steam)
    - [Portable wine](#portable-wine)
- [Usage Inside Docker](#usage-inside-docker)
- [Architecture](#architecture)
- [Further Considerations](#further-considerations)
- [Motivations](#motivations)
- [Related Projects](#related-projects)

## What is FlatImage?

FlatImage, is a hybrid of [Flatpak](https://github.com/flatpak/flatpak)
sandboxing with [AppImage](https://github.com/AppImage/AppImageKit) portability.

FlatImage use case is twofold:

* Flatimage is a package format, it includes a piece of software with all its
    dependencies for it work with across several linux distros (both Musl and
    GNU). Unlike `AppImage`, FlatImage runs the application in a container by
    default, which increases portability and compatibility at the cost of file
    size.

* Flatimage is a portable container image that requires no superuser permissions to run.

The diverse `GNU/Linux` ecosystem includes a vast array of distributions, each
with its own advantages and use cases. This can lead to cross-distribution
software compatibility challenges. FlatImage addresses these issues by:

* Utilizing its own root directory, enabling dynamic libraries with hard-coded
    paths to be packaged alongside the software without
    [binary patching](https://github.com/AppImage/AppImageKit/wiki/Bundling-Windows-applications).
* Running the application in its own gnu (or musl) environment, therefore, not using host
    libraries that might be outdated/incompatiblesystem with the application.

It simplifies the task of software packaging by enforcing the philosophy that it
should be as simple as setting up a container. This is an effort for the
end-user to not depend on the application developer to provide the portable
binary (or to handle how to package the application, dependencies and create a
runner script). It also increases the quality of life of the package developer,
simplifying the packaging process of applications.

## Comparison

| Feature                                                                   | FlatImage     | Docker                     | AppImage |
| :---                                                                      | :---:         | :---:                      | :---:    |
| No superuser privileges to use                                            | x             | x<sup>2</sup>              | x
| **Overlayfs** (allows to install programs even after compression)         | x             |                            |
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
* `FIM_FIFO`: If defined to 1, use FIFO when stdout/stderr are not connected to a terminal, default is 1.

### Read-Only

* `FIM_DIST`: The linux distribution name (alpine, arch, ubuntu)
* `FIM_FILE_BINARY`: Full path to the executed binary file
* `FIM_BASENAME_BINARY`: Basename of the executed binary file.
* `FIM_DIR_BINARY`: The path to the directory of the executed binary file.
* `FIM_DIR_TEMP`: Location of the temporary runtime directory.
* `FIM_DIR_MOUNT`: Location of the runtime fim mountpoint.
* `FIM_MAIN_OFFSET`: Shows filesystem offset and exits.
* `FIM_DIR_HOST_CONFIG`: Configuration directory in the host machine
* `FIM_DIR_HOST_OVERLAYS`: Overlayfs mountpoint directory in the host machine


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

#### Configure Application

You can use `./arch.flatimage fim-desktop` to get the following usage details:

```
fim-desktop:
   Configure the desktop integration
Usage:
   fim-desktop setup <json-file>
   fim-desktop enable <items...>
items:
   entry,mimetype,icon
Example:
   fim-desktop enable entry,mimetype,icon
```

To setup the desktop integration for a flatimage package, the first step is to
create a `json` file with the integration data, assume we create the file name
`desktop.json` with the following contents:

```json
{
  "name": "MyApp",
  "icon": "./my_app.png",
  "categories": ["System","Audio"]
}
```

This example creates the integration data for the application `MyApp`, with the
icon file `my_app.png` located in the same folder as `desktop.json`. The
categories field is used for the desktop menu entry integration, a list of valid
categories is found
[here](https://specifications.freedesktop.org/menu-spec/latest/category-registry.html#main-category-registry).
Let's assume the json file is called `desktop.json` and the flatimage file is
called `arch.flatimage`, the next command uses the `desktop.json` file to
configure the desktop integration.

```bash
$ ./arch.flatimage fim-desktop setup ./desktop.json
```

After the setup step, you can enable the integration selectively, `entry` refers
to the desktop entry in the start menu, `mimetype` refers to the file type that
appears in the file manager, `icon` is the application icon shown in the start
menu and the file manager. Here's how to enable everything:

```bash
$ ./arch.flatimage fim-desktop enable entry,mimetype,icon
```

#### Erase entries

To erase all desktop entries and icons created by flatimage, you can use the
command:

```bash
$ find ~/.local/share -iname "*flatimage*" -exec rm -v "{}" \;
```

#### XDG-Open

Flatimage redirects `xdg-open` commands to the host machine

Examples:

* Open a video file with the host default video player: `xdg-open my-file.mkv`
* Open a link with the host default browser: `xdg-open www.google.com`

### Overlayfs

Overlayfs is automatically enabled for all compressed filesystems.

### Hooks

You can include `bash` hooks to execute code before and/or after your program:

```bash
 # Include hook
$ ./arch.flatimage fim-hook-add-pre ./my-pre-hook
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
 # Include hook
$ ./arch.flatimage fim-hook-add-post ./my-post-hook
```

## Portals

Flatimage has a portal mechanism to dispatch commands to the host machine.

Examples:
* Open thunar in the host machine: `fim_portal_guest thunar`
* Open thunar in the host machine (full path): `fim_portal_guest /bin/thunar`
* Open the desktop folder with thunar on the host machine: `fim_portal_guest thunar ~/Desktop`

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
