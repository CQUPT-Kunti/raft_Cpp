#pragma once

#include <grpcpp/grpcpp.h>
#include "raft.grpc.pb.h"
#include "configNet.grpc.pb.h"

class RaftNode;

using grpc::Status;

class RaftServiceImpl final : public raft::RaftService::Service
{
public:
    Status SendMessage(grpc::ServerContext *context,
                       const configs::MessageRequest *request,
                       configs::MessageResponse *response) override;

    Status RequestVote(grpc::ServerContext *context,
                       const configs::RequestVoteRequest *request,
                       configs::RequestVoteResponse *response) override;
    RaftServiceImpl(RaftNode &node_);

    void Startgrpc();
    void InitStubs();
    void BroadcastMessage(std::string msg);

private:
    std::map<std::string, std::unique_ptr<raft::RaftService::Stub>> peers;
    RaftNode &node;
    grpc::ServerBuilder build;
    std::unique_ptr<grpc::Server> server_;
};
