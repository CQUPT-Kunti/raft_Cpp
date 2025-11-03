#pragma once

#include "configArgs.h"
#include "raftService.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include "raft.grpc.pb.h"

class RaftNode
{
private:
    netArgs net_args;
    NodeArgs node_args;
    int nodeId;
    std::vector<netArgs> group;
    RaftServiceImpl service;
    std::unique_ptr<grpc::Server> server_;
    std::map<std::string, std::unique_ptr<raft::RaftService::Stub>> peers;

public:
    RaftNode(netArgs args, int node_id, std::vector<netArgs> arg_s);
    void StartService();
    std::vector<netArgs> getGroup();
    netArgs getNetArgs();
    void BroadcastMessage(const std::string &content);
    void InitStubs();
};
