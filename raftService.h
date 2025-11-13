#pragma once

#include <grpcpp/grpcpp.h>
#include "raft.grpc.pb.h"
#include "configNet.grpc.pb.h"

#include "configArgs.h"
#include <future>
#include <vector>
#include <thread>
#include <map>
#include <memory>
#include <string>

struct VoteResult
{
    std::string port;
    grpc::Status status;
    configs::RequestVoteResponse response;
};

class RaftNode;

using grpc::Status;

class RaftServiceImpl final : public raft::RaftService::Service
{
public:
    Status SendMessage(grpc::ServerContext *context,
                       const configs::MessageRequest *request,
                       configs::MessageResponse *response) override;

    Status RequestVote(grpc::ServerContext *context,
                       const configs::RequestVoteRequest *request,
                       configs::RequestVoteResponse *response) override;

    Status HeartSend(grpc::ServerContext *context,
                     const configs::AppendEntriesRequest *request,
                     configs::AppendEntriesResponse *response) override;
    RaftServiceImpl(RaftNode &node_);
    ~RaftServiceImpl();
    void Startgrpc();
    void InitStubs();
    void BroadcastMessage(std::string msg);
    void BroadcastMessageAsync(std::string msg);
    void Vote();
    bool checkLogUptodate(int term, int index);

private:
    std::thread serverThread_;
    std::map<std::string, std::unique_ptr<raft::RaftService::Stub>> peers;
    RaftNode &node;
    NodeArgs &tempNodeconfig;
    grpc::ServerBuilder build;
    std::unique_ptr<grpc::Server> server_;
};
