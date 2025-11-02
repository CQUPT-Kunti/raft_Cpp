cmake_minimum_required(VERSION 3.14)
project(raft_grpc_cluster)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Threads REQUIRED)

# proto 文件
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto")

# 生成文件输出路径
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(GENERATED_SRCS "")

foreach(proto ${PROTO_FILES})
    get_filename_component(proto_name ${proto} NAME_WE)
    set(proto_src "${GENERATED_DIR}/${proto_name}.pb.cc")
    set(grpc_src "${GENERATED_DIR}/${proto_name}.grpc.pb.cc")

    add_custom_command(
        OUTPUT ${proto_src}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --cpp_out=${GENERATED_DIR} -I ${CMAKE_CURRENT_SOURCE_DIR}/proto ${proto}
        DEPENDS ${proto}
        COMMENT "Generating protobuf files for ${proto_name}"
    )

    add_custom_command(
        OUTPUT ${grpc_src}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --grpc_out=${GENERATED_DIR}
             --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
             -I ${CMAKE_CURRENT_SOURCE_DIR}/proto
             ${proto}
        DEPENDS ${proto} gRPC::grpc_cpp_plugin
        COMMENT "Generating gRPC files for ${proto_name}"
    )

    list(APPEND GENERATED_SRCS ${proto_src} ${grpc_src})
endforeach()

add_custom_target(proto_gen ALL DEPENDS ${GENERATED_SRCS})

# 编译节点程序
add_executable(raft_node
    main.cpp
    raft.cpp          # 你的节点逻辑
    raftService.cpp
    ${GENERATED_SRCS} # 只加入 cpp 文件，不要头文件
)

add_dependencies(raft_node proto_gen)

target_include_directories(raft_node
    PRIVATE
    ${GENERATED_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(raft_node
    PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
)
