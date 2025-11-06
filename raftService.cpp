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
