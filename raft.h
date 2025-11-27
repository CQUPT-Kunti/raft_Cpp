#pragma once

#include "configArgs.h"
#include "raftService.h"
#include "ThreadPool.h"
#include "timeManager.h"
#include <mutex>
#include <memory>

class RaftServiceImpl;

class RaftNode
{
private:
    netArgs net_args;
    NodeArgs node_args;
    std::vector<netArgs> group;
    RaftServiceImpl *service;
    std::mutex mtx;

    std::unique_ptr<ThreadPool> thread_pool;
    std::unique_ptr<TimeManager> time_manager;

public:
    int nodeId;
    int voteNums;

    RaftNode(netArgs args, int node_id, std::vector<netArgs> arg_s);
    ~RaftNode();

    void StartService();

    std::vector<netArgs> &getGroup();
    netArgs &getNetArgs();
    NodeArgs &getNodeArgs();
    std::mutex &getMutex();

    RaftServiceImpl *getService();
};
