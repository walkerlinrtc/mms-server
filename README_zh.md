[🇨🇳 中文文档](README_zh.md) | [🇺🇸 English](README.md)

# MMS：一个基于 C++20 实现的高性能、多线程、多协程的实时流媒体服务器

## 简介

**MMS Server** 是一个采用 **C++20** 构建的高性能实时流媒体服务器，专为直播和低延迟通信场景设计。它支持多种主流流媒体协议，包括：

* **RTMP / RTMPS**
* **HTTP / HTTPS-FLV**
* **HTTP / HTTPS-HLS**
* **HTTP / HTTPS-TS**
* **HTTP / HTTPS-DASH**
* **RTSP / RTSPS**
* **WebRTC**

借助 **C++20 协程** 的强大能力，MMS 提高了效率与可维护性，显著简化了与传统高性能服务器架构中异步编程的复杂性。这也提升了系统的可扩展性与长期可维护性。

MMS 遵循 **MIT 许可证** 发布，并兼容 **FFmpeg**、**OBS**、**VLC** 和 **WebRTC** 等主流客户端。它支持媒体采集、协议转换和分发，采用典型的 **发布（push）** 和 **订阅（play）** 模型。例如，接收到的 RTMP 流可转换为 HLS、HTTP-FLV 或 WebRTC 等格式进行播放。

MMS 主要面向直播场景，支持 RTMP、HLS、HTTP-FLV 以及 WebRTC 协议中的 **WHIP** 和 **WHEP**，适合作为实时音视频传输系统的核心组件。它可轻松与其他开源工具集成，构建完整的流媒体解决方案。

为便于系统集成和运维，MMS 提供了完整的 **HTTP API** 用于状态监控，并支持 **HTTP 回调**，用于集成鉴权、事件通知、DVR 控制等自定义业务逻辑。

MMS 对开发者友好，推荐在 **Ubuntu 22.04** 或 **Rocky Linux 9.5** 环境下进行开发与测试。

> 📘 文档地址：[http://www.mms-server.tech/zh/](http://www.mms-server.tech/zh/)

---

## 使用 CMake 编译

### 编译要求

* 目前仅支持 **Linux** 平台。
* 需使用完全支持 **C++20** 的 **GCC 编译器**（推荐使用 GCC 13 或更高版本）。
* 需安装 **CMake 3.25.0** 或更高版本。
* 需安装开发工具：`automake`、`autoconf`、`libtool`。
* 编译 **BoringSSL** 需要 **Go**。
* 编译 **libav** 需要 **NASM/YASM**。

如果从源码编译 GCC（如 13.1+ 版本），请确保配置参数正确。可使用 `g++ -v` 验证。典型的配置命令如下：

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

配置错误可能会导致链接失败。详见此 issue：
[https://github.com/jbl19860422/mms-server/issues/2](https://github.com/jbl19860422/mms-server/issues/2)

---

### 从源码构建

```bash
git clone https://github.com/jbl19860422/mms-server
cd mms-server
mkdir build && cd build
cmake .. && make -j4 install
```

> ⚠️ 编译过程需要稳定的网络连接，第三方库（如 Boost、FFmpeg）将在此过程中下载，可能需要一定时间。

---

### 配置说明

编译完成后，执行文件 `mms-live-server` 位于 `mms-server/bin` 目录下。

运行前请确保正确配置。MMS 使用 **YAML 格式** 的配置文件。为简化部署，默认**不包含域名配置**，你必须至少配置一个发布域名，否则推流时将返回 **403 Forbidden** 错误。

配置目录结构如下：

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

* `config/` — 主配置目录。

  * `mms.yaml`：全局服务配置（RTMP、HTTP、RTSP、WebRTC 等端口设置）。
* `publish/` — 发布域名的配置（例如 `test1.publish.com.yaml`）。
* `play/` — 播放域名的配置（例如 `test1.play.com.yaml`）。

每个发布域名可以关联多个播放域名。当接收到播放请求时，系统将验证是否在合法发布域名下存在对应流。如存在，则允许播放；否则返回 **404 Not Found**。

> 📘 详细配置示例请参考 \[xxxxx]。

---

### 启动服务

```bash
mms-live-server -c ./config -d
```

* `-c` 指定配置目录。
* `-d` 启用日志输出到控制台（省略该选项将输出到日志文件）。

### 使用 XMake 编译

详见 [xmake\_guide.md](xmake_guide.md)。

---

### 单实例服务控制台

---

## 📦 使用 `mms-server` 部署 Web 控制台

`mms-server` 内置静态文件服务器，可直接在服务中部署基于 Vue 的 Web 控制台。

### 1. 修改 `mms.yaml` 配置

开启静态文件服务器并设置控制台路径映射：

```yaml
http_api:
  enabled: true
  port: 8080
  static_file_server:
    enabled: true
    path_map:
      /console/*: /data/console
```

该配置将 `/console/*` 的请求映射至本地目录 `/data/console`。

---

### 2. 构建并部署控制台

#### 🔹 克隆前端项目

进入 `console` 目录并初始化子模块：

```bash
cd console
git submodule update --init --recursive
```

#### 🔹 编译项目

安装依赖并构建 Vue 应用：

```bash
npm install
npm run build
```

构建完成后将在当前目录生成 `console` 构建目录。

#### 🔹 部署构建产物

将构建好的 `console` 文件夹复制到配置路径下：

```bash
cp -rf console /data/
```

确保 `/data/console` 与上面配置的 `path_map` 相匹配。

---

### 3. 访问控制台

控制台部署完成后可通过以下地址访问：

```text
http://<your-ip>:8080/console/index.html
```

将 `<your-ip>` 和 `8080` 替换为你在 `http_api` 中配置的实际 IP 和端口。

---

## 📦 联系方式

1. mms-server QQ 群：1053583153
2. 个人微信/电话：13066836861
3. 邮箱：[jbl19860422@gmail.com](mailto:jbl19860422@gmail.com) 或 [jbl19860422@163.com](mailto:jbl19860422@163.com)
