## Introduction

MMS SERVER is a high-performance, real-time streaming media server developed in C++20, designed for live streaming and low-latency communication scenarios. It supports multiple mainstream streaming protocols, including:

* RTMP / RTMPS
* HTTP / HTTPS-FLV
* HTTP / HTTPS-HLS
* HTTP / HTTPS-TS
* RTSP / RTSPS
* WebRTC

MMS leverages C++20 coroutines to create an efficient, maintainable real-time streaming server. Compared to traditional asynchronous high-performance servers, it reduces code maintenance complexity and improves scalability.

MMS is licensed under the open-source MIT License and is compatible with clients like FFmpeg, OBS, VLC, and WebRTC. It has capabilities for media acquisition, reception, protocol conversion, and distribution. It adopts a typical publish (push) and subscribe (play) service model, allowing efficient protocol conversion between different formats, such as converting RTMP streams to HLS, HTTP-FLV, or WebRTC for playback.

MMS is primarily designed for live streaming scenarios, supporting protocols like RTMP, HLS, and HTTP-FLV for live streaming, and WHIP and WHEP for WebRTC. It can serve as a core media server, combined with various open-source tools to build a complete audio and video solution.

For system integration and maintenance, MMS provides a complete HTTP API for status queries and supports HTTP Callback, making it easy to integrate authentication, event notifications, and custom business logic (e.g., DVR control).

MMS is developer-friendly, recommended for development and debugging on Ubuntu 22.04 or Rocky linux9.5.

for docs please refer to http://www.mms-server.tech/en/.
## Compilation

### Requirements

* Currently only supports compilation in a Linux environment.
* Requires a GCC compiler supporting C++20, such as gcc-13 or higher.
* Requires cmake version 3.25.0 or higher.
* Requires automake autoconf libtoool.
* Requires golang for build libboringssl.
* Requires nasm/yasm for build libav.
* If compiling g++ 13.1 or later from source, use g++ -v to confirm the compilation options used during configuration:
```
../configure --enable-bootstrap --enable-languages=c,c++,lto --prefix=/root/gcc-13.1 --with-bugurl=https://bugs.rockylinux.org/ --enable-shared --enable-threads=posix --enable-checking=release --disable-multilib --with-system-zlib --enable-__cxa_atexit --disable-libunwind-exceptions --enable-gnu-unique-object --enable-linker-build-id --with-gcc-major-version-only --enable-libstdcxx-backtrace --with-linker-hash-style=gnu --enable-plugin --enable-initfini-array --enable-offload-targets=nvptx-none --without-cuda-driver --enable-offload-defaulted --enable-gnu-indirect-function --enable-cet --with-tune=generic --with-arch_64=x86-64-v2 --with-build-config=bootstrap-lto --enable-link-serialization=1
```
Otherwise, the following linking errors may occur. For reference, see:
https://github.com/jbl19860422/mms-server/issues/2

### Source Compilation

```
git clone https://github.com/jbl19860422/mms-server
cd mms-server
mkdir build && cd build
cmake .. && cmake -j4 install
```

Ensure a stable network connection during compilation, as third-party libraries like boost and ffmpeg need to be downloaded, which may take some time.

### Configuration

After compilation, the mms-live-server executable file will be generated in the mms-server/bin directory. Before running it, understand the configuration. The MMS configuration file is in YAML format. To simplify setup, there is no default domain configuration; you must configure the domain yourself before pushing streams, or you will receive a 403 error.

Configuration directory structure:

```
config/
├── mms.yaml
├── publish/
│   ├── test1.publish.com.yaml
│   └── test2.publish.com.yaml
├── play/
│   ├── test1.play.com.yaml
│   └── test2.play.com.yaml
```

* config is the main configuration directory. mms.yaml is the top-level configuration file for configuring server ports for rtmp/http/rtsp/webrtc.
* publish is the directory for publishing domain configuration files, e.g., test1.publish.com.yaml represents the configuration file for the domain test1.publish.com.
* play is the directory for playing domain configuration files, e.g., test1.play.com.yaml represents the configuration file for the domain test1.play.com.
* A publishing domain can provide playback for multiple play domains. When a playback request is received, the system checks whether the corresponding stream is available under the publishing domain. If available, playback succeeds; otherwise, a 404 error is returned.

For detailed configuration instructions, please refer to \[xxxxx].

### Startup

Execute:

```
mms-live-server -c ./config -d
```

This will start the server. The -d option outputs logs to the console. Without it, logs will be written to log files.

### Single Server Console

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
cp -r dist /data/console
```

Make sure `/data/console` matches the `path_map` configuration above.

---

### 3. Access the Console

Once deployed, the console can be accessed at:

```
http://<your-ip>:8080/console/index.html
```

Replace `<your-ip>` and `8080` with the actual IP address and port defined in your `http_api` configuration.



