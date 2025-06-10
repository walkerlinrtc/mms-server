# 🛠️ Use mms-server with xmake

## 📥 Install xmake

The following examples are for Ubuntu and Arch Linux. For other systems, please refer to the [xmake official installation guide](https://xmake.io/#/guide/installation).

### 🐧 Ubuntu 24.04

```bash
sudo add-apt-repository ppa:xmake-io/xmake
sudo apt update
sudo apt install xmake
```

Additionally, please ensure that the `pkg-config` tool is installed. Because `xmake` needs to use `pkg-config` to obtain configuration information for dependency libraries.

```bash
sudo apt install pkg-config
```

### 🐧 Arch Linux

```bash
sudo pacman -S xmake
```

For Arch Linux, please ensure that the `pkgconf` tool is installed. You can install the `base-devel` package to get the `pkgconf` tool.

```bash
sudo pacman -S base-devel
```

## 🏗️ Build mms-server

- Clone the project locally

```bash
git clone https://github.com/mms-server/mms-server.git
```

- Enter the project root directory

```bash
cd mms-server
```

- Build the project

```bash
xmake -j8 # Adjust the number of cores based on your system, large projects recommend using 8+ cores
```

`xmake` will automatically download and install third-party dependencies, then proceed with compilation. Typical output is as follows:

```bash
~/mms-server main !4
❯ xmake -j8
checking for platform ... linux
checking for architecture ... x86_64
updating repositories .. ok
note: install or modify (m) these packages (pass -y to skip confirm)?
in xmake-repo:
  -> spdlog v1.15.3 [license:MIT]
  -> boost 1.88.0 [asio:y, coroutine:y, system:y, serialization:y, program_options:y, regex:y, context:y, t ..)
  -> yaml-cpp 0.8.0 [license:MIT]
  -> jsoncpp 1.9.6 [license:MIT]
  -> go 1.24.4 [host, from:boringssl, license:BSD-3-Clause]
  -> boringssl 2022.06.13
  -> libopus 1.5.2
  -> autoconf 2.72 [host, from:automake,faac,faad2,libtool]
  -> automake 1.17 [host, from:faad2,faac]
  -> libtool 2.5.4 [host, from:faad2,faac]
  -> faac 1.30 [license:LGPL-2.1]
  -> faad2 2.10.0 [license:GPL-2.0]
  -> nasm 2.16.03 [host, from:ffmpeg, license:BSD-2-Clause]
  -> libffi 3.4.8 [private, host, from:python, license:MIT]
  -> ca-certificates 20250131 [host, from:python]
  -> python 3.13.2 [host, binary, from:ninja,meson, license:PSF]
  -> meson 1.8.1 [host, from:libdrm, license:Apache-2.0]
  -> ninja v1.12.1 [host, from:libdrm, license:Apache-2.0]
  -> libdrm 2.4.123 [from:ffmpeg, license:MIT]
  -> ffmpeg 7.1 [license:GPL-3.0]
  -> srtp v2.7
  -> hiredis v1.3.0 [from:redis-plus-plus, license:BSD-3-Clause]
  -> redis-plus-plus 1.3.14
please input: y (y/n/m)
```

Enter `y` to start downloading and installing third-party dependencies. This process may be affected by network conditions and machine configuration. Typical output is as follows:

```bash
  => download https://github.com/gabime/spdlog/archive/refs/tags/v1.15.3.zip .. ok
  ... 
  => install redis-plus-plus 1.3.14 .. ok
  => install python 3.13.2 .. ok
  => download https://github.com/ninja-build/ninja/archive/refs/tags/v1.12.1.tar.gz .. ok
  => download https://github.com/mesonbuild/meson/releases/download/1.8.1/meson-1.8.1.tar.gz .. ok
  => install meson 1.8.1 .. ok
  => install ninja v1.12.1 .. ok
  => download https://dri.freedesktop.org/libdrm/libdrm-2.4.123.tar.xz .. ok
  => install libdrm 2.4.123 .. ok
  => download https://ffmpeg.org/releases/ffmpeg-7.1.tar.bz2 .. ok
  => install ffmpeg 7.1 .. ok
```

```bash
[  5%]: cache compiling.release libs/protocol/mp4/smhd.cpp
[  5%]: cache compiling.release libs/protocol/mp4/stbl.cpp
[  5%]: cache compiling.release libs/protocol/mp4/mdat.cpp
[  5%]: cache compiling.release libs/protocol/mp4/url.cpp
[  5%]: cache compiling.release libs/protocol/mp4/visual_sample_entry.cpp
...
[ 93%]: archiving.release libmms-base.a
[ 94%]: archiving.release libmms-http.a
[ 94%]: archiving.release libmms-mp4.a
[ 94%]: archiving.release libmms-rtmp.a
[ 94%]: archiving.release libmms-rtp.a
[ 95%]: archiving.release libmms-rtsp.a
[ 95%]: archiving.release libmms-sdp.a
[ 95%]: archiving.release libmms-ts.a
[ 96%]: archiving.release libmms-codec.a
[ 96%]: archiving.release libmms-server.a
[ 99%]: archiving.release libmms-http-sdk.a
[ 99%]: linking.release mms-live-server
create ok!
compile_commands.json updated!
[100%]: build ok, spent 215.306s
```

## 🚀 Run mms-server (WIP)

In `xmake`, you need to use `xmake run` to run a target.

For this project, you can run it using the following command:

```bash
xmake run mms-live-server -c <absolute_path_to_config> -d
```

## ⚙️ VSCode configuration (WIP)

It is recommended to use VSCode as your editor and install the following extensions for a better development experience:

- `C/C++`
- `Clang-Format`
- `clangd`
- `CodeLLDB`
- `XMake`

## 📋 TODO

- [ ] Guide for installing mms-server via `xmake`
- [ ] VSCode configuration

## 🔗 Reference

- [xmake](https://xmake.io/#/)
- [xrepo](https://xrepo.xmake.io/#/)
