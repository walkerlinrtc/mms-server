include(ExternalProject)
include_directories(${PROJECT_BINARY_DIR}/include)
link_directories(${PROJECT_BINARY_DIR}/lib)
link_directories(${PROJECT_BINARY_DIR}/lib64)

ExternalProject_Add(libspdlog
    EXCLUDE_FROM_ALL 1
    GIT_REPOSITORY https://gitee.com/jbl19860422/spdlog.git
    GIT_TAG v1.14.0
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND cmake -B build -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}
    BUILD_COMMAND make -j4 -C build 
    INSTALL_COMMAND make -C build install
)

ExternalProject_Add(libjemalloc
    EXCLUDE_FROM_ALL 1
    URL https://github.com/jemalloc/jemalloc/archive/refs/tags/5.3.0.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ./autogen.sh && ./configure --prefix=${PROJECT_BINARY_DIR} --enable-static
    BUILD_COMMAND make
    INSTALL_COMMAND make install
)

ExternalProject_Add(libboost
    EXCLUDE_FROM_ALL 1
    # URL https://archives.boost.io/release/1.82.0/source/boost_1_82_0.tar.gz
    URL https://ulive1.cn-gd.ufileos.com/boost_1_82_0.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ./bootstrap.sh --prefix=${PROJECT_BINARY_DIR}
    BUILD_COMMAND ./b2 cxxflags="-std=c++20" --with-program_options --with-system --with-thread --with-date_time --with-regex --with-serialization --with-context --with-coroutine link=static
    INSTALL_COMMAND ./b2 install --prefix=${PROJECT_BINARY_DIR}
)
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)

ExternalProject_Add(libyamlcpp
    EXCLUDE_FROM_ALL 1
    URL https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.7.0.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND cmake -B build -DYAML_CPP_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}
    BUILD_COMMAND make -C build 
    INSTALL_COMMAND make -C build install
)

ExternalProject_Add(libjsoncpp
    EXCLUDE_FROM_ALL 1
    URL https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/1.8.4.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND cmake -B build -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}
    BUILD_COMMAND make -C build
    INSTALL_COMMAND make -C build install
)

ExternalProject_Add(libboringssl
    EXCLUDE_FROM_ALL 1
    GIT_REPOSITORY https://gitee.com/jbl19860422/boringssl.git
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND mkdir -p build && cd build && cmake .. -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}
    BUILD_COMMAND make -C build
    INSTALL_COMMAND make -C build install
)

INCLUDE_DIRECTORIES(${MMS_SOURCE_DIR}/third_src/spdlog/include)

ExternalProject_Add(libzlib
    EXCLUDE_FROM_ALL 1
    BUILD_IN_SOURCE 1
    GIT_REPOSITORY https://gitee.com/jbl19860422/zlib
    GIT_TAG v1.2.13
    CONFIGURE_COMMAND cmake . -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR}
    BUILD_COMMAND make
    INSTALL_COMMAND make install
)

ExternalProject_Add(libopus
    EXCLUDE_FROM_ALL 1
    URL https://archive.mozilla.org/pub/opus/opus-1.3.1.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ./configure --prefix=${PROJECT_BINARY_DIR}
    BUILD_COMMAND make
    INSTALL_COMMAND make install
)

ExternalProject_Add(libaac
    EXCLUDE_FROM_ALL 1
    GIT_REPOSITORY https://gitee.com/jbl19860422/faac.git
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ./bootstrap --prefix=${PROJECT_BINARY_DIR} && ./configure
    BUILD_COMMAND make
    INSTALL_COMMAND make install
)

ExternalProject_Add(libfaad2
    EXCLUDE_FROM_ALL 1
    GIT_REPOSITORY https://gitee.com/jbl19860422/faad2.git
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND cmake . -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR} -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release
    BUILD_COMMAND make
    INSTALL_COMMAND make install
)

ExternalProject_Add(libav-5.1.4
    EXCLUDE_FROM_ALL 1
    GIT_REPOSITORY https://gitee.com/mirrors/ffmpeg.git
    GIT_TAG n5.1.4
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ./configure --disable-programs --prefix=${PROJECT_BINARY_DIR}
    BUILD_COMMAND make -j4
    INSTALL_COMMAND make install
)

ExternalProject_Add(libsrtp
    EXCLUDE_FROM_ALL 1
    URL https://github.com/cisco/libsrtp/archive/refs/tags/v2.4.2.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ./configure --prefix=${PROJECT_BINARY_DIR}
    BUILD_COMMAND make
    INSTALL_COMMAND make install
)

ExternalProject_Add(libhiredis
    EXCLUDE_FROM_ALL 1
    GIT_REPOSITORY https://gitee.com/jbl19860422/hiredis.git
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND cmake . -B build -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR} -DCMAKE_C_FLAGS="-fPIC" -DBUILD_SHARED_LIBS=OFF
    BUILD_COMMAND make -j2 -C build
    INSTALL_COMMAND make install -C build
)

ExternalProject_Add(libredis++
    DEPENDS libhiredis
    EXCLUDE_FROM_ALL 1
    BUILD_IN_SOURCE 1
    GIT_REPOSITORY https://gitee.com/jbl19860422/redis-plus-plus
    CONFIGURE_COMMAND cmake . -B build -DCMAKE_INSTALL_PREFIX=${PROJECT_BINARY_DIR} -DREDIS_PLUS_PLUS_BUILD_TEST=OFF -DREDIS_PLUS_PLUS_BUILD_SHARED=OFF -DCMAKE_PREFIX_PATH=${PROJECT_BINARY_DIR}
    BUILD_COMMAND make -j2 -C build
    INSTALL_COMMAND make install -C build
)

include_directories(${PROJECT_BINARY_DIR}/include/sw)
