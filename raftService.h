#pragma once

#include <grpcpp/grpcpp.h>
#include "raft.grpc.pb.h"

class RaftNode;

using grpc::Status;

class RaftServiceImpl final : public raft::RaftService::Service
{
public:
    Status SendMessage(grpc::ServerContext *context,
                       const raft::MessageRequest *request,
                       raft::MessageResponse *response) override;
    RaftServiceImpl(RaftNode &node_);

private:
    RaftNode &node;
};
