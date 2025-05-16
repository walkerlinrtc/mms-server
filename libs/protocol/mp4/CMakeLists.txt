#source files
file(GLOB_RECURSE MMS_MP4_LIB_SOURCES    "${CMAKE_CURRENT_SOURCE_DIR}/*.h" 
                                        "${CMAKE_CURRENT_SOURCE_DIR}/*.c" 
                                        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" 
                                        "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

add_library(mms-mp4 STATIC ${MMS_MP4_LIB_SOURCES})
add_dependencies(mms-mp4 
    libboost
    libjsoncpp
    libboringssl
    mms-base
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(mms-mp4
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_include_directories(mms-mp4 
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


target_link_libraries(mms-mp4
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
    pthread
    mms-base
)

install(TARGETS mms-mp4 ARCHIVE DESTINATION lib)