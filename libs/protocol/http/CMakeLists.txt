#source files
file(GLOB_RECURSE MMS_HTTP_LIB_SOURCES  "${CMAKE_CURRENT_SOURCE_DIR}/*.h" 
                                        "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp" 
                                        "${CMAKE_CURRENT_SOURCE_DIR}/*.c" 
                                        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" 
                                        "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

add_library(mms-http STATIC ${MMS_HTTP_LIB_SOURCES})
add_dependencies(mms-http 
    libboost
    libjsoncpp
    libboringssl
    mms-base
)

target_include_directories(mms-http 
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(mms-http
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
    mms-base
    pthread
)

install(TARGETS mms-http ARCHIVE DESTINATION lib)