if (CMAKE_BUILD_TYPE STREQUAL Debug)
    set(CMAKE_CXX_FLAGS "-g -O0 -Werror -Wno-error=array-bounds -Wno-error=maybe-uninitialized -Wno-error=restrict")
else()
    set(CMAKE_CXX_FLAGS "-g -O2 -minline-all-stringops -Wall -Wextra -Werror -Wno-error=mismatched-new-delete")
endif()

# 生成版本头文件
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h
    @ONLY
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-gc-sections")

#source files
file(GLOB_RECURSE LIVE_SERVER_SOURCES   "${CMAKE_CURRENT_SOURCE_DIR}/*.h" 
                                        "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp" 
                                        "${CMAKE_CURRENT_SOURCE_DIR}/*.c" 
                                        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" 
                                        "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

add_executable(mms-live-server ${LIVE_SERVER_SOURCES})
add_dependencies(mms-live-server
    mms-base
    mms-rtmp
    mms-http-sdk
    mms-rtp
    mms-codec
    mms-ts
    mms-rtsp
    mms-sdp
    mms-mp4
    mms-server
    libspdlog
    libboost
    libjsoncpp
    libboringssl
    libsrtp
    libyamlcpp
    libopus
    libav-5.1.4
    libaac
    libfaad2
    libzlib
)

target_link_libraries(mms-live-server
    mms-server
    mms-base
    mms-http-sdk
    mms-rtmp
    mms-codec
    mms-ts
    mms-rtsp
    mms-rtp
    mms-mp4
    mms-sdp
    spdlog.a
    libfaac.a
    libfaad.a
    ssl.a 
    crypto.a
    libjsoncpp.a
    libz.a
    libyaml-cpp.a
    libopus.a
    libswresample.a
    libavutil.a
    libsrtp2.a
    boost_system.a
    boost_serialization.a
    boost_date_time.a
    boost_regex.a
    boost_coroutine.a
    boost_thread.a
    boost_context.a
    boost_program_options.a
    dl
    pthread
    asan
)
link_directories("/usr/lib64")
target_link_options(mms-live-server PRIVATE "-Wl,--no-gc-sections")
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS mms-live-server RUNTIME DESTINATION bin)
