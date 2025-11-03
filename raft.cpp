#include "raft.h"
#include <iostream>
#include <grpcpp/grpcpp.h>
#include <thread>

RaftNode::RaftNode(netArgs args, int node_id, std::vector<netArgs> arg_s) : service(*this)
{
    net_args = args;
    nodeId = node_id;
    group = arg_s;

    node_args.state = NodeState::Follower;

    node_args.currentTerm = 0;
    node_args.votedFor = -1;
    node_args.log.clear();

    node_args.commitIndex = node_args.lastApplied = 0;
}

std::vector<netArgs> RaftNode::getGroup()
{
    return group;
}

netArgs RaftNode::getNetArgs()
{
    return net_args;
}

void RaftNode::StartService()
{

    timeEvent.addRandomTimer(150, 300, [this]()
                             { std::cout << "⏰ Node " << nodeId << ": election timeout triggered!" << std::endl; });

    std::thread([this]()
                {
        while (true) {
            timeEvent.pollOnce(); // 只检查当前是否有事件
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } })
        .detach();
    std::string address = net_args.ip + ":" + net_args.port;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server_ = builder.BuildAndStart();
    std::cout << "Node " << nodeId << " listening on " << address << std::endl;
    InitStubs();
    BroadcastMessage("ni hao");
    server_->Wait();
}

void RaftNode::InitStubs()
{
    for (const auto &peer : group)
    {
        if (peer == net_args)
        {
            continue;
        }
        std::string target = peer.ip + ":" + peer.port;
        peers[peer.port] = raft::RaftService::NewStub(
            grpc::CreateChannel(target, grpc::InsecureChannelCredentials()));
    }
}

void RaftNode::BroadcastMessage(const std::string &content)
{
    configs::MessageRequest request;
    request.set_from(net_args.ip + ":" + net_args.port);
    request.set_content("test");
    for (const auto &[port, stub] : peers)
    {
        configs::MessageResponse response;
        grpc::ClientContext context;
        grpc::Status status = stub->SendMessage(&context, request, &response);
        if (status.ok())
        {
            std::cout << "Message sent to " << port
                      << ", reply: " << response.reply() << std::endl;
        }
        else
        {
            std::cerr << "Failed to send to " << port
                      << ", error: " << status.error_message() << std::endl;
        }
    }
}
