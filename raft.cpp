#include "raft.h"
#include <iostream>
#include <grpcpp/grpcpp.h>
#include <thread>

RaftNode::RaftNode(netArgs args, int node_id, std::vector<netArgs> arg_s) : service(*this)
{
    net_args = args;
    nodeId = node_id;
    group = arg_s;

    node_args.state = NodeState::Follower;

    voteNums = 0;
    node_args.currentTerm = 0;
    node_args.votedFor = -1;
    node_args.log.clear();

    node_args.commitIndex = node_args.lastApplied = 0;
}

std::vector<netArgs> RaftNode::getGroup()
{
    return group;
}

netArgs RaftNode::getNetArgs()
{
    return net_args;
}

void RaftNode::StartService()
{
    std::string address = net_args.ip + ":" + net_args.port;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server_ = builder.BuildAndStart();
    std::cout << "Node " << nodeId << " listening on " << address << std::endl;
    InitStubs();

    timeEvent.addRandomTimer(150, 300, [this]()
                             { this->Vote(); });

    vote_thread = std::thread([this]()
                              {
        while (true)
        {
            timeEvent.pollOnce(); // 只检查当前是否有事件
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } });
    vote_thread.detach();
    server_->Wait();
}

NodeArgs &RaftNode::getNodeArgs()
{
    return node_args;
}
void RaftNode::InitStubs()
{
    for (const auto &peer : group)
    {
        if (peer == net_args)
        {
            continue;
        }
        std::string target = peer.ip + ":" + peer.port;
        peers[peer.port] = raft::RaftService::NewStub(
            grpc::CreateChannel(target, grpc::InsecureChannelCredentials()));
    }
}

std::mutex &RaftNode::getMutex()
{
    return mtx;
}
bool RaftNode::checkLogUptodate(int term, int index)
{
    if (node_args.log.empty())
        return true;

    if (term > node_args.log.back().term)
    {
        return true;
    }

    if (term == node_args.log.back().term && index >= node_args.log.size())
    {
        return true;
    }

    return false;
}

void RaftNode::BroadcastMessage(const std::string &content)
{
    configs::MessageRequest request;
    request.set_from(net_args.ip + ":" + net_args.port);
    request.set_content("test");
    for (const auto &[port, stub] : peers)
    {
        configs::MessageResponse response;
        grpc::ClientContext context;
        std::cout << " ==== 1 ==== " << std::endl;
        grpc::Status status = stub->SendMessage(&context, request, &response);
        std::cout << " ==== 2 ==== " << std::endl;

        if (status.ok())
        {
            std::cout << "Message sent to " << port
                      << ", reply: " << response.reply() << std::endl;
        }
        else
        {
            std::cerr << "Failed to send to " << port
                      << ", error: " << status.error_message() << std::endl;
        }
    }
}

void RaftNode::Vote()
{

    // 领导者不进行二次选举，只进行心跳
    if (node_args.state == NodeState::Leader)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lk(mtx);
        node_args.currentTerm += 1; // 新任期
        node_args.votedFor = -1;    // 自己投自己
    }
    voteNums += 1; // 自己一票
    int totalNodes = (int)peers.size() + 1;
    int majority = totalNodes / 2 + 1;
    configs::RequestVoteRequest request;
    {
        std::lock_guard<std::mutex> lk(mtx);
        request.set_term(node_args.currentTerm);
        request.set_condidateid(nodeId);
        int lastIndex = node_args.log.empty() ? -1 : (int)node_args.log.size() - 1;
        int lastTerm = node_args.log.empty() ? 0 : node_args.log.back().term;
        request.set_lastlogindex(lastIndex);
        request.set_lastlogterm(lastTerm);
    }

    for (auto &[port, stub] : peers)
    {
        configs::RequestVoteResponse response;
        grpc::ClientContext context;
        auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(300);
        context.set_deadline(deadline);
        grpc::Status status = stub->RequestVote(&context, request, &response);
        if (status.ok())
        {
            if (response.votegranted())
            {
                voteNums += 1;
            }
            // 如果对端的 term 更大，退回 follower 并更新本地 term
            int remoteTerm = response.term();
            std::lock_guard<std::mutex> lk(mtx);
            if (remoteTerm > node_args.currentTerm)
            {
                node_args.currentTerm = remoteTerm;
                node_args.state = NodeState::Follower;
                node_args.votedFor = -1;
                return;
            }
        }
        else
        {
            std::cerr << "RequestVote to " << port << " failed: " << status.error_message() << std::endl;
        }

        if (voteNums >= majority)
        {
            {
                std::lock_guard<std::mutex> lk(mtx);
                node_args.state = NodeState::Leader;
                std::cout << (net_args.ip + ":" + net_args.port + " 我是 Leader 我的票数：" + std::to_string(voteNums)) << std::endl;
                node_args.votedFor = nodeId;
            }
            StartHeartbeatThread();
            return;
        }
    }
}

void RaftNode::StartHeartbeatThread()
{
    heat_thread = std::thread([this]()
                              {
                                  while (true)
                                  {
                                      std::lock_guard<std::mutex> lk(mtx);
                                      if (node_args.state != NodeState::Leader)
                                            break; 
                                  // operation
                                      std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                  } });
    heat_thread.detach();
}

void RaftNode::ResetElectionTimer()
{
    int timeout_fd = timeEvent.addRandomTimer(150, 300, [this]()
                                              {
                                                  this->Vote(); // 超时触发选举
                                              });
}