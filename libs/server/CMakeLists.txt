#source files
file(GLOB_RECURSE MMS_SERVER_LIB_SOURCES    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp" 
                                            "${CMAKE_CURRENT_SOURCE_DIR}/*.h" 
                                            "${CMAKE_CURRENT_SOURCE_DIR}/*.c" 
                                            "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" 
                                            "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

add_library(mms-server STATIC ${MMS_SERVER_LIB_SOURCES})
add_dependencies(mms-server 
    libboost
    libjsoncpp
    libboringssl
)

target_include_directories(mms-server 
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)