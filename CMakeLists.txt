cmake_minimum_required(VERSION 3.15)
project(RaftSystem)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# 生成protobuf和gRPC文件
set(PROTO_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/proto/raft.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/proto/configNet.proto
)

foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_PATH ${PROTO_FILE} PATH)
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    
    set(PROTO_SRC "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.cc")
    set(PROTO_HDR "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.h")
    set(GRPC_SRC "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.cc")
    set(GRPC_HDR "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.h")
    
    add_custom_command(
        OUTPUT ${PROTO_SRC} ${PROTO_HDR} ${GRPC_SRC} ${GRPC_HDR}
        COMMAND protobuf::protoc
        ARGS --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
             --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
             -I${PROTO_PATH}
             --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
             ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
    )
    
    list(APPEND PROTO_SRCS ${PROTO_SRC} ${GRPC_SRC})
    list(APPEND PROTO_HDRS ${PROTO_HDR} ${GRPC_HDR})
endforeach()

# 源文件
set(SOURCE_FILES
    main.cpp
    election.cpp
    heartbeat.cpp
    raft_node.cpp
    log_replication.cpp
    thread_pool.cpp
    time_manager.cpp
    timer_pool.cpp
    timer.cpp
    ${PROTO_SRCS}
)

add_executable(raft_server ${SOURCE_FILES})

target_include_directories(raft_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(raft_server
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
)
