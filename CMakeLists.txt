cmake_minimum_required(VERSION 3.14)
project(raft_grpc_cluster)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(asio-grpc CONFIG REQUIRED)


# proto 文件
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto")

# 生成文件输出路径
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(GENERATED_SRCS "")

# 为每个 proto 文件生成 protobuf + gRPC C++ 文件
foreach(proto ${PROTO_FILES})
    get_filename_component(proto_name ${proto} NAME_WE)

    set(proto_cpp "${GENERATED_DIR}/${proto_name}.pb.cc")
    set(proto_h "${GENERATED_DIR}/${proto_name}.pb.h")
    set(grpc_cpp "${GENERATED_DIR}/${proto_name}.grpc.pb.cc")
    set(grpc_h "${GENERATED_DIR}/${proto_name}.grpc.pb.h")

    # protobuf 文件生成
    add_custom_command(
        OUTPUT ${proto_cpp} ${proto_h}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --cpp_out=${GENERATED_DIR}
             -I ${CMAKE_CURRENT_SOURCE_DIR}/proto
             ${proto}
        DEPENDS ${proto}
        COMMENT "Generating protobuf C++ for ${proto_name}"
    )

    # gRPC 文件生成
    add_custom_command(
        OUTPUT ${grpc_cpp} ${grpc_h}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --grpc_out=${GENERATED_DIR}
             --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
             -I ${CMAKE_CURRENT_SOURCE_DIR}/proto
             ${proto}
        DEPENDS ${proto} gRPC::grpc_cpp_plugin
        COMMENT "Generating gRPC C++ for ${proto_name}"
    )

    list(APPEND GENERATED_SRCS ${proto_cpp} ${grpc_cpp})
endforeach()

# 生成 proto 文件的虚拟 target
add_custom_target(proto_gen ALL DEPENDS ${GENERATED_SRCS})

# 可执行文件
add_executable(raft_node
    main.cpp
    raft.cpp
    raftService.cpp
    ${GENERATED_SRCS}   # 添加生成的 cpp 文件
)

# 保证 raft_node 编译前先生成 proto
add_dependencies(raft_node proto_gen)

# include 目录
target_include_directories(raft_node
    PRIVATE
    ${GENERATED_DIR}         # 生成的 pb.h 和 grpc.pb.h
    ${CMAKE_CURRENT_SOURCE_DIR}  # 你的源码目录
)

# 链接库
target_link_libraries(raft_node
    PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
    asio-grpc::asio-grpc
)
