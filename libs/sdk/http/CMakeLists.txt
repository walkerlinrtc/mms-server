#source files
file(GLOB_RECURSE MMS_HTTP_SDK_LIB_SOURCES  "${CMAKE_CURRENT_SOURCE_DIR}/*.h" 
                                        "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp" 
                                        "${CMAKE_CURRENT_SOURCE_DIR}/*.c" 
                                        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" 
                                        "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

add_library(mms-http-sdk STATIC ${MMS_HTTP_SDK_LIB_SOURCES})
add_dependencies(mms-http-sdk 
    libboost
    libjsoncpp
    libboringssl
    mms-http
)

target_include_directories(mms-http-sdk
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(mms-http-sdk
    boost_system.a
    boost_serialization.a
    boost_date_time.a
    boost_regex.a
    boost_coroutine.a
    boost_thread.a
    boost_context.a
    boost_program_options.a
    ssl.a 
    crypto.a
    libjsoncpp.a
    libz.a
    libyaml-cpp.a
    dl
    mms-http
    pthread
)

install(TARGETS mms-http-sdk ARCHIVE DESTINATION lib)