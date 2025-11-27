#pragma once

#include <map>
#include <memory>
#include <thread>
#include <vector>
#include <future>
#include <grpcpp/grpcpp.h>
#include "raft.grpc.pb.h"
#include "configNet.grpc.pb.h"
#include "configArgs.h"

class RaftNode;

// 投票结果结构体
struct VoteResult
{
    std::string port;
    grpc::Status status;
    configs::RequestVoteResponse response;
};

class RaftServiceImpl final : public raft::RaftService::Service
{
public:
    RaftServiceImpl(RaftNode &node_);
    ~RaftServiceImpl();

    grpc::Status SendMessage(grpc::ServerContext *context,
                             const configs::MessageRequest *request,
                             configs::MessageResponse *response) override;

    grpc::Status RequestVote(grpc::ServerContext *context,
                             const configs::RequestVoteRequest *request,
                             configs::RequestVoteResponse *response) override;

    grpc::Status HeartSend(grpc::ServerContext *context,
                           const configs::AppendEntriesRequest *request,
                           configs::AppendEntriesResponse *response) override;

    void Startgrpc();
    void InitStubs();
    void Vote();
    void Heart();

private:
    bool checkLogUptodate(int term, int index);

    std::thread serverThread_;
    std::map<std::string, std::unique_ptr<raft::RaftService::Stub>> peers;
    RaftNode &node;
    NodeArgs &tempNodeconfig;
    grpc::ServerBuilder build;
    std::unique_ptr<grpc::Server> server_;
};
