set_project("mms-server")
set_xmakever("2.9.8")

set_languages("c++20")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate")

-- 若不设置, 则默认编译模式为 release
-- set_defaultmode("debug")

-- 第三方依赖
add_requires("spdlog 1.15.3")
add_requires("boost 1.88.0", { configs = { asio = true, program_options = true, system = true, thread = true, date_time = true, regex = true, serialization = true, context = true, coroutine = true }})
add_requires("yaml-cpp 0.8.0")
add_requires("jsoncpp 1.9.6")
add_requires("boringssl 2022.06.13")
add_requires("zlib 1.3.1")
add_requires("libopus 1.5.2")
add_requires("faac 1.30")
add_requires("faad2 2.10.0")
add_requires("ffmpeg 7.1")
add_requires("srtp 2.7")
add_requires("leveldb 1.23")
add_requires("jemalloc 5.3.0")

-- 添加头文件包含路径
add_includedirs("libs")
add_includedirs("live-server")

-- 添加子目录
includes("libs")
includes("live-server")