#include "raftService.h"

#include <iostream>

#include <grpcpp/grpcpp.h>

#include "configArgs.h"

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
    return Status::OK;
}