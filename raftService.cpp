#include "raftService.h"
#include "raft.h"

#include <random>
#include <iostream>
#include <grpcpp/grpcpp.h>

Status RaftServiceImpl::SendMessage(grpc::ServerContext *context,
                                    const configs::MessageRequest *request,
                                    configs::MessageResponse *response)
{
    std::cout << "Received message from: " << request->from() << std::endl;
    std::cout << "Message content: " << request->content() << std::endl;
    response->set_reply("Hello from RaftServiceImpl");
    return Status::OK;
}

RaftServiceImpl::RaftServiceImpl(RaftNode &node_) : node(node_), tempNodeconfig(node_.getNodeArgs()) {}

RaftServiceImpl::~RaftServiceImpl()
{
    if (serverThread_.joinable())
        serverThread_.join();
}

Status RaftServiceImpl::RequestVote(grpc::ServerContext *context,
                                    const configs::RequestVoteRequest *request,
                                    configs::RequestVoteResponse *response)
{
    {
        std::unique_lock<std::shared_mutex> lock(node.getMutex());
        response->set_votegranted(false);
        response->set_term(tempNodeconfig.currentTerm);
        if (request->term() < tempNodeconfig.currentTerm)
        {
            return Status::OK;
        }

        if (request->term() > tempNodeconfig.currentTerm)
        {
            tempNodeconfig.state = NodeState::Follower;
            tempNodeconfig.currentTerm = request->term();
            tempNodeconfig.votedFor = -1;
        }

        if ((tempNodeconfig.votedFor == -1 || tempNodeconfig.votedFor == request->condidateid()) &&
            checkLogUptodate(request->lastlogterm(), request->lastlogindex()))
        {
            tempNodeconfig.votedFor = request->condidateid();
            response->set_votegranted(true);
        }

        response->set_term(tempNodeconfig.currentTerm);
        return Status::OK;
    }
}

Status RaftServiceImpl::HeartSend(grpc::ServerContext *context,
                                  const configs::AppendEntriesRequest *request,
                                  configs::AppendEntriesResponse *response)
{
    return Status::OK;
}

void RaftServiceImpl::Startgrpc()
{
    netArgs &tempNet = node.getNetArgs();
    std::string address = tempNet.ip + ":" + tempNet.port;
    build.AddListeningPort(address, grpc::InsecureServerCredentials());
    build.RegisterService(this);
    server_ = build.BuildAndStart();
    if (server_)
        std::cout << "Node " << node.nodeId << " listening on " << address << std::endl;
    else
    {
        std::cerr << "Failed to start server on " << address << std::endl;
        return;
    }
    InitStubs();

    Vote();

    serverThread_ = std::thread([this]()
                                { server_->Wait(); });
}

void RaftServiceImpl::InitStubs()
{
    auto tempGroup = node.getGroup();
    for (const auto &peer : tempGroup)
    {
        if (peer == node.getNetArgs())
            continue;
        std::string target = peer.ip + ":" + peer.port;
        peers[peer.port] = raft::RaftService::NewStub(
            grpc::CreateChannel(target, grpc::InsecureChannelCredentials()));
    }
}

void randomSleepMicroseconds(int min_us, int max_us)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min_us, max_us);

    int sleepTime = dis(gen);
    std::this_thread::sleep_for(std::chrono::microseconds(sleepTime));
}

void RaftServiceImpl::Vote()
{
    randomSleepMicroseconds(300000, 600000);
    if (tempNodeconfig.state == NodeState::Leader)
        return;

    {
        std::unique_lock<std::shared_mutex> lock(node.getMutex());
        tempNodeconfig.state = NodeState::Candidate;
        tempNodeconfig.currentTerm += 1;
        tempNodeconfig.votedFor = node.nodeId;
        node.voteNums += 1;
    }

    std::vector<std::future<VoteResult>> results;

    configs::RequestVoteRequest request;
    {
        std::unique_lock<std::shared_mutex> lock(node.getMutex());
        request.set_term(tempNodeconfig.currentTerm);
        request.set_condidateid(node.nodeId);
        request.set_lastlogindex(tempNodeconfig.log.empty() ? 0 : tempNodeconfig.log.size());
        request.set_lastlogterm(tempNodeconfig.log.empty() ? 0 : tempNodeconfig.log.back().term);
    }

    for (const auto &[port, stub] : peers)
    {
        results.push_back(std::async(std::launch::async, [stub = stub.get(),
                                                          port,
                                                          request]()
                                     { grpc::ClientContext context;
                                     configs::RequestVoteResponse response; 
                                     grpc::Status status = stub->RequestVote(&context, request, &response);
                                    return VoteResult{port,status,response}; }));
    }

    for (auto &f : results)
    {
        VoteResult result = f.get();
        if (result.response.votegranted() == true)
        {
            {
                std::unique_lock<std::shared_mutex> lock(node.getMutex());
                node.voteNums += 1;
            }
            std::cout << node.getNetArgs().port << "accpet a page from" << result.port << " nums is " << node.voteNums << std::endl;
        }
    }
}

bool RaftServiceImpl::checkLogUptodate(int term, int index)
{
    if (tempNodeconfig.log.size() == 0)
    {
        return true;
    }

    if (term > tempNodeconfig.log.back().term)
    {
        return true;
    }

    if (term == tempNodeconfig.log.back().term && index >= tempNodeconfig.log.size())
    {
        return true;
    }

    return false;
}