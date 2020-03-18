[![Build Status](https://github.com/tsoding/vodus/workflows/CI/badge.svg)](https://github.com/tsoding/vodus/actions)

# Vodus

Introduction Video: https://www.twitch.tv/videos/563056420

**WARNING! The application is in an active development state and is not even
alpha yet. Use it at your own risk. Nothing is documented, anything can be
changed at any moment or stop working at all.**

## Quick Start

```console
$ # Install Dependencies
$ ## Debian
$ sudo apt-get install libfreetype6-dev libgif-dev libpng-dev libavutil-dev libavcodec-dev
$ ## NixOS
$ nix-shell
$ ## (add your distro here)
$ # Build
$ make            # build vodus executable
$ make render     # test rendering (produces output.mpeg file)
```
