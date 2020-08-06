[![Build Status](https://github.com/tsoding/vodus/workflows/CI/badge.svg)](https://github.com/tsoding/vodus/actions)

# Vodus

Introduction Video: https://www.twitch.tv/videos/563056420

**WARNING! The application is in an active development state and is not even
alpha yet. Use it at your own risk. Nothing is documented, anything can be
changed at any moment or stop working at all.**

## Quick Start

### Install Dependencies

#### Debian

```console
$ sudo apt-get install nasm libfreetype6-dev libcurl4-openssl-dev
```

#### NixOS

```console
$ nix-shell
```

#### FreeBSD
```console
# pkg install nasm xmlto freetype2 curl openssl gcc gmake
```

### Build Third Party dependencies

Needs to be done only once.

``` console
$ cd third_party/
$ ./build_third_party.sh
$ cd ..
```

### Build the Project

```console
$ make
```

### Download Emotes

```console
$ ./emote_downloader
```

### Render the Sample Video

```console
$ make render
```
