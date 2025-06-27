[🇨🇳 中文文档](README_zh.md) | [🇺🇸 English](README.md)

# MMS: A high-performance, multi-threaded, and multi-coroutine real-time streaming server implemented in C++20

## Introduction

**MMS Server** is a high-performance, real-time streaming media server built with **C++20**, designed specifically for live streaming and low-latency communication scenarios. It supports a wide range of mainstream streaming protocols, including:

* **RTMP / RTMPS**
* **HTTP / HTTPS-FLV**
* **HTTP / HTTPS-HLS**
* **HTTP / HTTPS-TS**
* **HTTP / HTTPS-DASH**
* **RTSP / RTSPS**
* **WebRTC**

Leveraging the power of **C++20 coroutines**, MMS achieves high efficiency and maintainability, significantly reducing the complexity of asynchronous programming compared to traditional high-performance server architectures. This also enhances scalability and long-term maintainability.

MMS is released under the **MIT License** and is compatible with popular clients such as **FFmpeg**, **OBS**, **VLC**, and **WebRTC**. It supports media ingestion, protocol conversion, and distribution in a typical **publish (push)** and **subscribe (play)** model. For example, an incoming RTMP stream can be converted to HLS, HTTP-FLV, or WebRTC formats for playback.

Designed primarily for live streaming, MMS supports RTMP, HLS, HTTP-FLV, as well as WebRTC protocols like **WHIP** and **WHEP**, making it suitable as a core component of a real-time audio and video delivery system. It can be easily integrated with other open-source tools to build a complete streaming solution.

For system integration and operations, MMS provides a comprehensive **HTTP API** for status monitoring and supports **HTTP callbacks** to integrate features like authentication, event notifications, and custom business logic (e.g., DVR control).

MMS is developer-friendly and recommended for development and testing on **Ubuntu 22.04** or **Rocky Linux 9.5**.

> 📘 Documentation is available at: [http://www.mms-server.tech/en/](http://www.mms-server.tech/en/)

---

## Compilation with CMake

### Requirements

* Currently supports **Linux** platforms only.
* Requires a **GCC compiler** with full **C++20** support (GCC 13 or higher recommended).
* Requires **CMake** version **3.25.0** or above.
* Requires development tools: `automake`, `autoconf`, `libtool`.
* Requires **Go** for building **BoringSSL**.
* Requires **NASM/YASM** for building **libav**.

If compiling GCC (e.g., version 13.1+) from source, ensure it is configured with appropriate options. You can verify with `g++ -v`. A typical configuration command might look like:

```bash
../configure --enable-bootstrap --enable-languages=c,c++,lto --prefix=/root/gcc-13.1 \
--with-bugurl=https://bugs.rockylinux.org/ --enable-shared --enable-threads=posix \
--enable-checking=release --disable-multilib --with-system-zlib \
--enable-__cxa_atexit --disable-libunwind-exceptions --enable-gnu-unique-object \
--enable-linker-build-id --with-gcc-major-version-only --enable-libstdcxx-backtrace \
--with-linker-hash-style=gnu --enable-plugin --enable-initfini-array \
--enable-offload-targets=nvptx-none --without-cuda-driver --enable-offload-defaulted \
--enable-gnu-indirect-function --enable-cet --with-tune=generic \
--with-arch_64=x86-64-v2 --with-build-config=bootstrap-lto --enable-link-serialization=1
```

Incorrect configuration may lead to linking errors. For more information, refer to this issue:
[https://github.com/jbl19860422/mms-server/issues/2](https://github.com/jbl19860422/mms-server/issues/2)

---

### Building from Source

```bash
git clone https://github.com/jbl19860422/mms-server
cd mms-server
mkdir build && cd build
cmake .. && make -j4 install
```

> ⚠️ Ensure a stable internet connection during the build process, as third-party libraries (e.g., Boost, FFmpeg) will be downloaded. This may take some time.

---

### Configuration

After compilation, the executable `mms-live-server` will be located in the `mms-server/bin` directory.

Before running, ensure proper configuration. MMS uses **YAML** format for its configuration files. To simplify deployment, **no default domain** is provided — you must configure at least one publishing domain; otherwise, streaming attempts will result in **403 Forbidden** errors.

Configuration directory structure:

```text
config/
├── mms.yaml
├── publish/
│   ├── test1.publish.com.yaml
│   └── test2.publish.com.yaml
├── play/
│   ├── test1.play.com.yaml
│   └── test2.play.com.yaml
```

* `config/` — Main configuration directory.

  * `mms.yaml`: Global server settings (ports for RTMP, HTTP, RTSP, WebRTC, etc.)
* `publish/` — Configuration files for publishing domains (e.g., `test1.publish.com.yaml`).
* `play/` — Configuration files for playback domains (e.g., `test1.play.com.yaml`).

Each publishing domain can serve multiple playback domains. When a playback request is received, the system checks whether the corresponding stream exists under a valid publishing domain. If found, playback is allowed; otherwise, a **404 Not Found** error is returned.

> 📘 For detailed configuration examples, please refer to \[xxxxx].

---

### Running the Server

```bash
mms-live-server -c ./config -d
```

* `-c` specifies the configuration directory.
* `-d` enables log output to the console (omit this flag to write logs to files instead).

### Compilation with XMake

Please refer to [xmake_guide.md](xmake_guide.md) for more information.

---

### Single Server Console

---

## Docker Compose Quick Deployment Guide
本项目提供基于 Docker Compose 的一键部署方式，目前 docker 镜像使用 Ubuntu24.04，适合 mac 等非 Linux 平台用户进行环境部署。

### 1. Modify Configuration Files

In the `docker-compose.yaml` file, update the mount path `<local mms-server path>` and container name `<container name>` to your local directory and desired container name:

```yaml
services:
  <container name>:
...
    container_name: <container name>
...
    volumes:
      - <local mms-server path>:/mnt/workspace/mms-server
...
```

(Optional) In the `Dockerfile`, you may change the default Ubuntu username `myuser` and password `password` to your custom credentials:

```dockerfile
...
# Add a non-root user and grant sudo permissions
RUN useradd -ms /bin/bash myuser && \
    echo "myuser:password" | chpasswd && \
    usermod -aG sudo myuser

# Install xmake (as root)
RUN add-apt-repository -y ppa:xmake-io/xmake && \
    apt update && \
    apt install -y xmake

# Switch to the non-root user
USER myuser
WORKDIR /home/myuser
...
```

---

### 2. Build and Run the Container

Run the following commands in the project root directory:

```bash
# Start the container in the background (will build on first run)
docker compose up -d
# Enter the container shell
docker exec -it <container name> bash
# Stop the container
docker compose stop
```

Once inside the container, you can run `xmake -j8` to automatically install project dependencies and build the project.

---

## 📦 Deploying the Web Console with `mms-server`

The `mms-server` includes a built-in static file server, allowing you to host the standalone Vue-based web console directly within the same server environment.

### 1. Configure `mms.yaml`

To enable the static file server and map the console path, update your `mms.yaml` as follows:

```yaml
http_api:
  enabled: true
  port: 8080
  static_file_server:
    enabled: true
    path_map:
      /console/*: /data/console
```

This configuration maps requests to `/console/*` to the local directory `/data/console`.

---

### 2. Build and Deploy the Console

#### 🔹 Clone the frontend project

Navigate to the `console` directory and initialize any submodules:

```bash
cd console
git submodule update --init --recursive
```

#### 🔹 Build the project

Install dependencies and compile the Vue application:

```bash
npm install
npm run build
```

This will generate a `console` build directory inside the current folder.

#### 🔹 Deploy the build

Copy the generated `console` directory to the target path configured in `mms.yaml`:

```bash
cp -rf console /data/
```

Make sure `/data/console` matches the `path_map` configuration above.

---

### 3. Access the Console

Once deployed, the console can be accessed at:

```text
http://<your-ip>:8080/console/index.html
```

Replace `<your-ip>` and `8080` with the actual IP address and port defined in your `http_api` configuration.

---

## 📦 Contact

1. mms-server QQ Group: 1053583153
2. Personal Contact: 13066836861
3. Email: <jbl19860422@gmail.com> or <jbl19860422@163.com>
