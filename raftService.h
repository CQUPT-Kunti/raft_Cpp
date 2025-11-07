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

    Status HeartSend(grpc::ServerContext *context,
                     const configs::AppendEntriesRequest *request,
                     configs::AppendEntriesResponse *response) override;
    RaftServiceImpl(RaftNode &node_);

private:
    RaftNode &node;
};
