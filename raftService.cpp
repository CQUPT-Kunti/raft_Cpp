#include "raftService.h"
#include "raft.h"
#include <iostream>
#include <random>
#include <thread>

RaftServiceImpl::RaftServiceImpl(RaftNode &node_)
    : node(node_), tempNodeconfig(node_.getNodeArgs())
{
}

RaftServiceImpl::~RaftServiceImpl()
{
    if (server_)
    {
        server_->Shutdown();
    }
    if (serverThread_.joinable())
    {
        serverThread_.join();
    }
}

grpc::Status RaftServiceImpl::SendMessage(grpc::ServerContext *context,
                                          const configs::MessageRequest *request,
                                          configs::MessageResponse *response)
{
    std::cout << "Received message from: " << request->from() << std::endl;
    std::cout << "Message content: " << request->content() << std::endl;
    response->set_reply("Hello from RaftServiceImpl");
    return grpc::Status::OK;
}

grpc::Status RaftServiceImpl::RequestVote(grpc::ServerContext *context,
                                          const configs::RequestVoteRequest *request,
                                          configs::RequestVoteResponse *response)
{
    std::unique_lock<std::mutex> lock(node.getMutex());

    response->set_votegranted(false);
    response->set_term(tempNodeconfig.currentTerm);

    // 如果已经投票给别人，拒绝
    if (tempNodeconfig.state == NodeState::Follower && tempNodeconfig.votedFor != -1)
    {
        return grpc::Status::OK;
    }

    // 如果请求的term小于当前term，拒绝
    if (request->term() < tempNodeconfig.currentTerm)
    {
        return grpc::Status::OK;
    }

    // 如果请求的term大于当前term，更新自己
    if (request->term() > tempNodeconfig.currentTerm)
    {
        tempNodeconfig.state = NodeState::Follower;
        tempNodeconfig.currentTerm = request->term();
        tempNodeconfig.votedFor = -1;
    }

    // 检查是否可以投票
    if ((tempNodeconfig.votedFor == -1 || tempNodeconfig.votedFor == request->condidateid()) &&
        checkLogUptodate(request->lastlogterm(), request->lastlogindex()))
    {
        tempNodeconfig.votedFor = request->condidateid();
        response->set_votegranted(true);
    }

    response->set_term(tempNodeconfig.currentTerm);
    return grpc::Status::OK;
}

grpc::Status RaftServiceImpl::HeartSend(grpc::ServerContext *context,
                                        const configs::AppendEntriesRequest *request,
                                        configs::AppendEntriesResponse *response)
{
    std::cout << node.getNetArgs().port << " has accept heart" << std::endl;
    return grpc::Status::OK;
}

void RaftServiceImpl::Startgrpc()
{
    netArgs &tempNet = node.getNetArgs();
    std::string address = tempNet.ip + ":" + tempNet.port;

    build.AddListeningPort(address, grpc::InsecureServerCredentials());
    build.RegisterService(this);
    server_ = build.BuildAndStart();

    if (server_)
    {
        std::cout << "Node " << node.nodeId << " listening on " << address << std::endl;
    }
    else
    {
        std::cerr << "Failed to start server on " << address << std::endl;
        return;
    }

    InitStubs();

    serverThread_ = std::thread([this]()
                                { server_->Wait(); });
}

void RaftServiceImpl::InitStubs()
{
    auto tempGroup = node.getGroup();
    for (const auto &peer : tempGroup)
    {
        if (peer.ip == node.getNetArgs().ip && peer.port == node.getNetArgs().port)
        {
            continue;
        }
        std::string target = peer.ip + ":" + peer.port;
        peers[peer.port] = raft::RaftService::NewStub(
            grpc::CreateChannel(target, grpc::InsecureChannelCredentials()));
    }
}

void RaftServiceImpl::Vote()
{
    // 如果已经是Leader，不需要选举
    if (tempNodeconfig.state == NodeState::Leader)
    {
        return;
    }

    // 转变为Candidate
    {
        std::unique_lock<std::mutex> lock(node.getMutex());
        tempNodeconfig.state = NodeState::Candidate;
        tempNodeconfig.currentTerm += 1;
        tempNodeconfig.votedFor = node.nodeId;
        node.voteNums = 1; // 给自己投一票
    }

    std::cout << "Node " << node.nodeId << " starts election for term "
              << tempNodeconfig.currentTerm << std::endl;

    // 准备投票请求
    configs::RequestVoteRequest request;
    {
        std::unique_lock<std::mutex> lock(node.getMutex());
        request.set_term(tempNodeconfig.currentTerm);
        request.set_condidateid(node.nodeId);
        request.set_lastlogindex(tempNodeconfig.log.empty() ? 0 : tempNodeconfig.log.size());
        request.set_lastlogterm(tempNodeconfig.log.empty() ? 0 : tempNodeconfig.log.back().term);
    }

    // 向所有peer发送投票请求
    std::vector<std::future<VoteResult>> results;
    for (const auto &pair : peers)
    {
        std::string port = pair.first;
        raft::RaftService::Stub *stub = pair.second.get();

        results.push_back(std::async(std::launch::async,
                                     [stub, port, request]() -> VoteResult
                                     {
                                         grpc::ClientContext context;
                                         configs::RequestVoteResponse response;
                                         grpc::Status status = stub->RequestVote(&context, request, &response);
                                         return VoteResult{port, status, response};
                                     }));
    }

    // 等待投票结果
    int follower_num = (peers.size() + 1) / 2 + 1; // 过半数

    for (auto &future : results)
    {
        VoteResult result = future.get();

        if (result.status.ok() && result.response.votegranted())
        {
            std::unique_lock<std::mutex> lock(node.getMutex());
            node.voteNums += 1;

            std::cout << "Node " << node.nodeId << " got vote from " << result.port
                      << " (" << node.voteNums << "/" << follower_num << ")" << std::endl;

            // 如果获得过半数投票，成为Leader
            if (node.voteNums >= follower_num && tempNodeconfig.state != NodeState::Leader)
            {
                tempNodeconfig.state = NodeState::Leader;
                std::cout << "Node " << node.nodeId << " became Leader in term "
                          << tempNodeconfig.currentTerm << std::endl;

                // 开始发送心跳
                Heart();
            }
        }
    }
}

void RaftServiceImpl::Heart()
{
    if (tempNodeconfig.state != NodeState::Leader)
    {
        return;
    }

    std::cout << "Node " << node.nodeId << " sending heartbeat" << std::endl;

    configs::AppendEntriesRequest request;
    {
        std::unique_lock<std::mutex> lock(node.getMutex());
        request.set_term(tempNodeconfig.currentTerm);
        request.set_leaderid(node.nodeId);
    }

    // 异步发送心跳给所有peer
    for (const auto &pair : peers)
    {
        raft::RaftService::Stub *stub = pair.second.get();

        (void)std::async(std::launch::async, [stub, request]()
                         {
            grpc::ClientContext context;
            configs::AppendEntriesResponse response;
            stub->HeartSend(&context, request, &response); });
    }
}

bool RaftServiceImpl::checkLogUptodate(int term, int index)
{
    if (tempNodeconfig.log.empty())
    {
        return true;
    }

    if (term > tempNodeconfig.log.back().term)
    {
        return true;
    }

    if (term == tempNodeconfig.log.back().term && index >= (int)tempNodeconfig.log.size())
    {
        return true;
    }

    return false;
}
