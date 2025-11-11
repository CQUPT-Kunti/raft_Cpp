#pragma once

#include "configArgs.h"
#include "raftService.h"
#include <memory>
#include <thread>

#include <grpcpp/grpcpp.h>
#include "raft.grpc.pb.h"

class RaftNode
{
private:
    netArgs net_args;
    NodeArgs node_args;
    std::vector<netArgs> group;
    RaftServiceImpl service;
    std::unique_ptr<grpc::Server> server_;

public:
    int nodeId;

    RaftNode(netArgs args, int node_id, std::vector<netArgs> arg_s);
    void StartService();
    std::vector<netArgs> &getGroup();
    netArgs &getNetArgs();
    NodeArgs &getNodeArgs();
    void BroadcastMessage(const std::string &content);
    void InitStubs();
};
