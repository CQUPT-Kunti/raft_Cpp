#include "raft.h"
#include <iostream>
#include <grpcpp/grpcpp.h>

RaftNode::RaftNode(netArgs args, int node_id, std::vector<netArgs> arg_s) : service(*this)
{
    net_args = args;
    nodeId = node_id;
    group = arg_s;

    node_args.state = NodeState::Follower;

    node_args.currentTerm = 0;
    node_args.votedFor = -1;
    node_args.log.clear();

    node_args.commitIndex = node_args.lastApplied = 0;
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

void RaftNode::StartService()
{
    service.Startgrpc();
    std::cout << " 是否 已经 阻塞" << std::endl;
}