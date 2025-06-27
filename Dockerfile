FROM ubuntu:24.04

# 设置环境变量
ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US:en
ENV LC_ALL=en_US.UTF-8

# 更新包管理器，安装基础工具和 UTF-8 本地化支持
RUN apt update && apt install -y \
    vim \
    locales \
    sudo \
    passwd \
    cmake \
    g++ \
    curl \
    wget \
    pkg-config \
    unzip \
    autoconf \
    openssl \
    libssl-dev \
    iproute2 \
    software-properties-common && \
    locale-gen en_US.UTF-8 && \
    # 执行非最小化安装
    yes | unminimize

# 添加非 root 用户 myuser 并设置密码和 sudo 权限
RUN useradd -ms /bin/bash myuser && \
    echo "myuser:password" | chpasswd && \
    usermod -aG sudo myuser

# 安装 xmake（在 root 用户下添加 PPA）
RUN add-apt-repository -y ppa:xmake-io/xmake && \
    apt update && \
    apt install -y xmake

# 切换到非 root 用户
USER myuser
WORKDIR /home/myuser

# 暴露常用端口（仅作声明）
EXPOSE 80 1935 1936 443 554 555 8080 8878

# 默认 shell
SHELL ["/bin/bash", "-c"]
