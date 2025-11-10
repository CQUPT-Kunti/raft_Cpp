#include "raftService.h"

#include <iostream>

#include <grpcpp/grpcpp.h>

#include "configArgs.h"
#include "raft.h"

Status RaftServiceImpl::SendMessage(grpc::ServerContext *context,
                                    const configs::MessageRequest *request,
                                    configs::MessageResponse *response)
{
    std::cout << "Received message from: " << request->from() << std::endl;
    std::cout << "Message content: " << request->content() << std::endl;
    response->set_reply("Hello from RaftServiceImpl");
    return Status::OK;
}

RaftServiceImpl::RaftServiceImpl(RaftNode &node_) : node(node_) {}

Status RaftServiceImpl::RequestVote(grpc::ServerContext *context,
                                    const configs::RequestVoteRequest *request,
                                    configs::RequestVoteResponse *response)
{
    return Status::OK;
}

void RaftServiceImpl::Startgrpc()
{
    netArgs &tempNet = node.getNetArgs();
    std::string address = tempNet.ip + ":" + tempNet.port;
    build.AddListeningPort(address, grpc::InsecureServerCredentials());
    build.RegisterService(this);
    server_ = build.BuildAndStart();
    if (server_)
        std::cout << "Node " << node.nodeId << " listening on " << address << std::endl;
    else
    {
        std::cerr << "Failed to start server on " << address << std::endl;
        return;
    }
    InitStubs();
    BroadcastMessage("ni hao");
    server_->Wait();
}

void RaftServiceImpl::InitStubs()
{
    auto tempGroup = node.getGroup();
    for (const auto &peer : tempGroup)
    {
        if (peer == node.getNetArgs())
            continue;
        std::string target = peer.ip + ":" + peer.port;
        peers[peer.port] = raft::RaftService::NewStub(
            grpc::CreateChannel(target, grpc::InsecureChannelCredentials()));
    }
}

void RaftServiceImpl::BroadcastMessage(std::string msg)
{
    configs::MessageRequest request;
    netArgs &tempNet = node.getNetArgs();
    request.set_from(tempNet.ip + ":" + tempNet.port);
    request.set_content("test");
    for (const auto &[port, stub] : peers)
    {
        configs::MessageResponse response;
        grpc::ClientContext context;
        grpc::Status status = stub->SendMessage(&context, request, &response);
        if (status.ok())
            std::cout << "Message sent to " << port
                      << ", reply: " << response.reply() << std::endl;
        else
            std::cerr << "Failed to send to " << port
                      << ", error: " << status.error_message() << std::endl;
    }
}