#pragma once

#include "configArgs.h"
#include "time_loop.h"
#include "raftService.h"
#include "raft.grpc.pb.h"

#include <grpcpp/grpcpp.h>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
class RaftNode
{
private:
    TimeEvent timeEvent;
    netArgs net_args;
    NodeArgs node_args;
    int nodeId;
    std::vector<netArgs> group;
    RaftServiceImpl service;
    std::unique_ptr<grpc::Server> server_;
    std::map<std::string, std::unique_ptr<raft::RaftService::Stub>> peers;
    std::mutex mtx;

    int voteNums;

    // thread
    std::thread vote_thread;
    std::thread heat_thread;

public:
    RaftNode(netArgs args, int node_id, std::vector<netArgs> arg_s);
    void StartService();
    std::vector<netArgs> getGroup();
    netArgs getNetArgs();
    NodeArgs &getNodeArgs();
    void BroadcastMessage(const std::string &content);
    void InitStubs();
    std::mutex &getMutex();
    void Vote();

    void ResetElectionTimer();

    bool checkLogUptodate(int term, int index);
    // 心跳
    void StartHeartbeatThread();
};
