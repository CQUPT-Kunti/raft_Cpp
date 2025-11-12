#pragma once

#include "configArgs.h"
#include "raftService.h"
#include <memory>
#include <thread>
#include <shared_mutex>

#include <grpcpp/grpcpp.h>
#include "raft.grpc.pb.h"

class RaftServiceImpl;

class RaftNode
{
private:
    netArgs net_args;
    NodeArgs node_args;
    std::vector<netArgs> group;
    RaftServiceImpl service;
    std::unique_ptr<grpc::Server> server_;
    std::shared_mutex mtx;

public:
    int nodeId;
    int voteNums;
    std::shared_mutex  &getMutex();
    RaftNode(netArgs args, int node_id, std::vector<netArgs> arg_s);
    void StartService();
    std::vector<netArgs> &getGroup();
    netArgs &getNetArgs();
    NodeArgs &getNodeArgs();
    void BroadcastMessage(const std::string &content);
    void InitStubs();
    bool checkLogUptodate(int term, int index);
};
