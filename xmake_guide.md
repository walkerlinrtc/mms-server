# 🛠️ Use mms-server with xmake

## 📥 Install xmake

The following examples are for Ubuntu and Arch Linux. For other systems, please refer to the [xmake official installation guide](https://xmake.io/#/guide/installation).

### 🐧 Ubuntu 24.04

```bash
sudo add-apt-repository ppa:xmake-io/xmake
sudo apt update
sudo apt install xmake
```

### 🐧 Arch Linux

```bash
sudo pacman -S xmake
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

> ⚠️ The third-party dependency `boringssl` has been replaced with `openssl3`. Please refer to the [Note](#-note) section for details.

```bash
~/Workspace/mms-server main !13
❯ xmake -j8
checking for platform ... linux
checking for architecture ... x86_64
updating repositories .. ok
note: install or modify (m) these packages (pass -y to skip confirm)?
in xmake-repo:
  -> spdlog v1.15.3 [license:MIT]
  -> boost 1.88.0 [date_time:y, regex:y, system:y, serialization:y, asio:y, thread:y, coroutine:y, program_ ..)
  -> yaml-cpp 0.8.0 [license:MIT]
  -> jsoncpp 1.9.6 [license:MIT]
  -> go 1.24.4 [host, from:boringssl, license:BSD-3-Clause]
  -> boringssl 2022.06.13
  -> libopus 1.5.2
  -> autoconf 2.72 [host, from:faad2,automake,faac,libtool]
  -> automake 1.17 [host, from:faac,faad2]
  -> libtool 2.5.4 [host, from:faac,faad2]
  -> faac 1.30 [license:LGPL-2.1]
  -> faad2 2.10.0 [license:GPL-2.0]
  -> nasm 2.16.03 [host, from:ffmpeg, license:BSD-2-Clause]
  -> pkg-config 0.29.2 [host, from:ffmpeg]
  -> libffi 3.4.8 [private, host, from:python, license:MIT]
  -> openssl 1.1.1-w [host, from:python,srtp, license:Apache-2.0]
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
  => download https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/1.9.6.zip .. ok
  => download https://github.com/jbeder/yaml-cpp/archive/refs/tags/0.8.0.tar.gz .. ok
  => install spdlog v1.15.3 .. ok
  => install jsoncpp 1.9.6 .. ok
  => install yaml-cpp 0.8.0 .. ok
  => download https://mirrors.ustc.edu.cn/gnu/autoconf/autoconf-2.72.tar.gz .. ok
  => install autoconf 2.72 .. ok
  => download https://mirrors.ustc.edu.cn/gnu/automake/automake-1.17.tar.gz .. ok
  => download https://github.com/boostorg/boost/releases/download/boost-1.88.0/boost-1.88.0-cmake.tar.gz .. ok
  => download https://downloads.xiph.org/releases/opus/opus-1.5.2.tar.gz .. ok
  => download https://go.dev/dl/go1.24.4.linux-amd64.tar.gz .. ok
  => install go 1.24.4 .. ok
  => install automake 1.17 .. ok
  => download https://mirrors.ustc.edu.cn/gnu/libtool/libtool-2.5.4.tar.gz .. ok
  => download https://github.com/google/boringssl/archive/refs/tags/fips-20220613.tar.gz .. ok
  => install libopus 1.5.2 .. ok
  => download https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/nasm-2.16.03.tar.xz .. ok
  => install libtool 2.5.4 .. ok
  => download https://github.com/knik0/faac/archive/refs/tags/1_30.tar.gz .. ok
  => install faac 1.30 .. ok
  => install nasm 2.16.03 .. ok
  => install boringssl 2022.06.13 .. ok
  => download https://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz .. ok
  => download https://github.com/libffi/libffi/releases/download/v3.4.8/libffi-3.4.8.tar.gz .. ok
  => install boost 1.88.0 .. ok
  => download https://github.com/knik0/faad2/archive/refs/tags/2_10_0.tar.gz .. ok
  => install libffi 3.4.8 .. ok
  => download https://github.com/openssl/openssl/archive/refs/tags/OpenSSL_1_1_1w.zip .. ok
  => download https://github.com/xmake-mirror/xmake-cacert/archive/refs/tags/20250131.zip .. ok
  => install ca-certificates 20250131 .. ok
  => download https://github.com/redis/hiredis/archive/refs/tags/v1.3.0.tar.gz .. ok
  => install hiredis v1.3.0 .. ok
  => install openssl 1.1.1-w .. ok
  => download https://github.com/sewenew/redis-plus-plus/archive/refs/tags/1.3.14.tar.gz .. ok
  => download https://www.python.org/ftp/python/3.13.2/Python-3.13.2.tgz .. ok
  => install faad2 2.10.0 .. ok
  => download https://github.com/cisco/libsrtp/archive/refs/tags/v2.7.tar.gz .. ok
  => install redis-plus-plus 1.3.14 .. ok
  => install srtp v2.7 .. ok
  => install pkg-config 0.29.2 .. ok
  => installing python .. (1/make) ⠧
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

## 📝 Note

When using `xmake`, to avoid linking errors, the third-party dependency `boringssl` has been replaced with `openssl3`, which can be successfully compiled and linked.

If you insist on using `boringssl`, you may encounter the following linking errors:

```bash
error: /usr/bin/ld: build/linux/x86_64/release/libmms-server.a(tls_server.cpp.o): in function `mms::TlsServer::init_ssl()':
tls_server.cpp:(.text+0x5e9): undefined reference to `OpenSSL_add_all_algorithms'
/usr/bin/ld: tls_server.cpp:(.text+0x5ee): undefined reference to `SSL_library_init'
/usr/bin/ld: tls_server.cpp:(.text+0x5f3): undefined reference to `SSL_load_error_strings'
/usr/bin/ld: build/linux/x86_64/release/libmms-server.a(tls_server.cpp.o): in function `mms::TlsServer::start_listen(unsigned short)':
tls_server.cpp:(.text+0x321c): undefined reference to `OpenSSL_add_all_algorithms'
/usr/bin/ld: tls_server.cpp:(.text+0x3221): undefined reference to `SSL_library_init'
/usr/bin/ld: tls_server.cpp:(.text+0x3226): undefined reference to `SSL_load_error_strings'
/usr/bin/ld: build/linux/x86_64/release/libmms-base.a(tls_socket.cpp.o): in function `mms::TlsSocket::send(mms::TlsSocket::send(unsigned char const*, unsigned long, int)::_ZN3mms9TlsSocket4sendEPKhmi.Frame*) [clone .actor]':
tls_socket.cpp:(.text+0x4fb3): undefined reference to `BIO_get_mem_data'
/usr/bin/ld: tls_socket.cpp:(.text+0x510c): undefined reference to `BIO_reset'
/usr/bin/ld: build/linux/x86_64/release/libmms-base.a(tls_socket.cpp.o): in function `mms::TlsSocket::send(mms::TlsSocket::send(std::vector<boost::asio::const_buffer, std::allocator<boost::asio::const_buffer> >&, int)::_ZN3mms9TlsSocket4sendERSt6vectorIN5boost4asio12const_bufferESaIS4_EEi.Frame*) [clone .actor]':
tls_socket.cpp:(.text+0x54de): undefined reference to `BIO_get_mem_data'
/usr/bin/ld: tls_socket.cpp:(.text+0x566c): undefined reference to `BIO_reset'
/usr/bin/ld: build/linux/x86_64/release/libmms-base.a(tls_socket.cpp.o): in function `mms::TlsSocket::connect(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, unsigned short, int)::{lambda()#1}::operator()(mms::TlsSocket::connect(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, unsigned short, int)::{lambda()#1}::operator()() const::_ZZN3mms9TlsSocket7connectERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEtiENKUlvE_clEv.Frame*) [clone .actor]':
tls_socket.cpp:(.text+0x5c38): undefined reference to `BIO_reset'
/usr/bin/ld: tls_socket.cpp:(.text+0x5d2f): undefined reference to `BIO_get_mem_data'
/usr/bin/ld: build/linux/x86_64/release/libmms-base.a(tls_socket.cpp.o): in function `mms::TlsSocket::do_handshake(int)::{lambda()#1}::operator()(mms::TlsSocket::do_handshake(int)::{lambda()#1}::operator()() const::_ZZN3mms9TlsSocket12do_handshakeEiENKUlvE_clEv.Frame*) [clone .actor]':
tls_socket.cpp:(.text+0x6105): undefined reference to `BIO_reset'
/usr/bin/ld: tls_socket.cpp:(.text+0x616d): undefined reference to `BIO_reset'
/usr/bin/ld: tls_socket.cpp:(.text+0x6419): undefined reference to `BIO_get_mem_data'
/usr/bin/ld: tls_socket.cpp:(.text+0x6489): undefined reference to `BIO_get_mem_data'
/usr/bin/ld: build/linux/x86_64/release/libmms-base.a(tls_socket.cpp.o): in function `mms::TlsSocket::connect(mms::TlsSocket::connect(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, unsigned short, int)::_ZN3mms9TlsSocket7connectERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEti.Frame*) [clone .actor]':
tls_socket.cpp:(.text+0x9e62): undefined reference to `SSL_set_mode'
/usr/bin/ld: build/linux/x86_64/release/libmms-base.a(tls_socket.cpp.o): in function `mms::TlsSocket::do_handshake(mms::TlsSocket::do_handshake(int)::_ZN3mms9TlsSocket12do_handshakeEi.Frame*) [clone .actor]':
tls_socket.cpp:(.text+0xa60a): undefined reference to `SSL_CTX_set_tlsext_servername_arg'
/usr/bin/ld: tls_socket.cpp:(.text+0xa621): undefined reference to `SSL_CTX_set_tlsext_servername_callback'
/usr/bin/ld: tls_socket.cpp:(.text+0xa6ab): undefined reference to `SSL_set_mode'
collect2: error: ld returned 1 exit status
```

## 📋 TODO

- [ ] Guide for installing mms-server via `xmake`
- [ ] VSCode configuration

## 🔗 Reference

- [xmake](https://xmake.io/#/)
- [xrepo](https://xrepo.xmake.io/#/)
