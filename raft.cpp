#include "raft.h"
#include <iostream>
#include <grpcpp/grpcpp.h>

RaftNode::RaftNode(netArgs args, std::vector<netArgs> arg_s) : service(*this)
{
    net_args = args;
    nodeId = "2A";
    group = arg_s;
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
    raft::MessageRequest request;
    request.set_from(net_args.ip + ":" + net_args.port);
    request.set_content("test");
    for (const auto &[port, stub] : peers)
    {
        raft::MessageResponse response;
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
