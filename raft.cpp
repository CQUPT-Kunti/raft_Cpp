#include "raft.h"
#include "raftService.h"
#include <iostream>
#include <thread>

RaftNode::RaftNode(netArgs args, int node_id, std::vector<netArgs> arg_s)
{
    net_args = args;
    nodeId = node_id;
    group = arg_s;
    voteNums = 0;

    node_args.state = NodeState::Follower;
    node_args.currentTerm = 0;
    node_args.votedFor = -1;
    node_args.log.clear();
    node_args.commitIndex = 0;
    node_args.lastApplied = 0;

    service = new RaftServiceImpl(*this);
}

RaftNode::~RaftNode()
{
    if (service)
    {
        delete service;
    }
}

void RaftNode::StartService()
{
    // 创建线程池（4个工作线程）
    thread_pool = std::make_unique<ThreadPool>(4);

    // 创建时间管理器，预分配200个Timer
    time_manager = std::make_unique<TimeManager>(thread_pool.get(), 200);

    // 启动gRPC服务
    service->Startgrpc();

    std::cout << "Node " << nodeId << " service started" << std::endl;

    // 等待一下让gRPC服务启动
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 添加选举超时定时器（150-300ms）
    time_manager->addTimer(150, 300, [this]()
                           {
        if (this->node_args.state != NodeState::Leader) {
            this->service->Vote();
        } });

    // 如果是Leader，添加心跳定时器（每50ms）
    time_manager->addTimer(50, 50, [this]()
                           {
        if (this->node_args.state == NodeState::Leader) {
            this->service->Heart();
        } });

    // 定期打印状态（每5秒）
    time_manager->addTimer(5000, 5000, [this]()
                           { std::cout << "Node " << this->nodeId
                                       << " - State: " << (int)this->node_args.state
                                       << ", Term: " << this->node_args.currentTerm
                                       << ", Timer Pool: " << this->time_manager->getFreeTimerCount()
                                       << "/" << this->time_manager->getTotalTimerCount() << std::endl; });

    // 保持运行
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

std::vector<netArgs> &RaftNode::getGroup()
{
    return group;
}

netArgs &RaftNode::getNetArgs()
{
    return net_args;
}

NodeArgs &RaftNode::getNodeArgs()
{
    return node_args;
}

std::mutex &RaftNode::getMutex()
{
    return mtx;
}

RaftServiceImpl *RaftNode::getService()
{
    return service;
}
