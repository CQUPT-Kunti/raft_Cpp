#include "raftService.h"

#include <iostream>

#include <grpcpp/grpcpp.h>

#include "configArgs.h"
#include "raft.h"

Status RaftServiceImpl::SendMessage(grpc::ServerContext *context,
                                    const configs::MessageRequest *request,
                                    configs::MessageResponse *response)
{
    std::cout << "Received message from: " << request->from() << std::endl;
    std::cout << "Message content: " << request->content() << std::endl;
    response->set_reply("Hello from RaftServiceImpl");
    return Status::OK;
}

RaftServiceImpl::RaftServiceImpl(RaftNode &node_) : node(node_) {}

Status RaftServiceImpl::RequestVote(grpc::ServerContext *context,
                                    const configs::RequestVoteRequest *request,
                                    configs::RequestVoteResponse *response)
{
    response->set_votegranted(false);

    {
        std::lock_guard<std::mutex> lk(node.getMutex());
        NodeArgs &tempNode = node.getNodeArgs();

        if (request->term() < tempNode.currentTerm)
        {
            response->set_term(tempNode.currentTerm);
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "term is too small");
        }

        if (request->term() > tempNode.currentTerm)
        {
            tempNode.currentTerm = request->term();
            tempNode.state = NodeState::Follower;
            tempNode.votedFor = -1;
        }

        response->set_term(tempNode.currentTerm);

        if (tempNode.votedFor == -1 || tempNode.votedFor == request->condidateid())
        {
            bool upToDate = node.checkLogUptodate(request->lastlogterm(), request->lastlogindex());
            if (upToDate)
            {
                tempNode.votedFor = request->condidateid();
                response->set_votegranted(true);
            }
        }

        // operation
    }
    return Status::OK;
}

// 星跳发送
Status RaftServiceImpl::HeartSend(grpc::ServerContext *constext,
                                  const configs::AppendEntriesRequest *request,
                                  configs::AppendEntriesResponse *response)
{
    std::lock_guard<std::mutex> lock(node.getMutex());
    NodeArgs tempNodeConfig = node.getNodeArgs();

    std::cout << "=======  heart begin ========" << std::endl;

    if (request->term() < tempNodeConfig.currentTerm)
    {
        response->set_success(false);
        response->set_term(tempNodeConfig.currentTerm);
        return grpc::Status(grpc::INVALID_ARGUMENT, "leader's term is too small");
    }

    if (request->term() > tempNodeConfig.currentTerm)
    {
        tempNodeConfig.currentTerm = request->term();
        tempNodeConfig.state = NodeState::Follower;
        tempNodeConfig.votedFor = -1;
    }

    node.ResetElectionTimer();

    // 4. 更新 commitIndex（空心跳 entries.size()==0 也可以处理）
    if (request->leadercommit() > tempNodeConfig.commitIndex)
    {
        tempNodeConfig.commitIndex = std::min(request->leadercommit(), (int)tempNodeConfig.log.size() - 1);
    }

    std::cout << "=======  heart end ========" << std::endl;

    response->set_term(tempNodeConfig.currentTerm);
    response->set_success(true);
    return Status::OK;
}