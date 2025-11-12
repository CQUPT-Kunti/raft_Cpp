#include "raftService.h"
#include "raft.h"

#include <random>
#include <iostream>
#include <grpcpp/grpcpp.h>

Status RaftServiceImpl::SendMessage(grpc::ServerContext *context,
                                    const configs::MessageRequest *request,
                                    configs::MessageResponse *response)
{
    std::cout << "Received message from: " << request->from() << std::endl;
    std::cout << "Message content: " << request->content() << std::endl;
    response->set_reply("Hello from RaftServiceImpl");
    return Status::OK;
}

RaftServiceImpl::RaftServiceImpl(RaftNode &node_) : node(node_), tempNodeconfig(node_.getNodeArgs()) {}

RaftServiceImpl::~RaftServiceImpl()
{
    if (serverThread_.joinable())
        serverThread_.join();
}

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

    Vote();

    serverThread_ = std::thread([this]()
                                { server_->Wait(); });
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
