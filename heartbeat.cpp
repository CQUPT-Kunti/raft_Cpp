#include "heartbeat.h"
#include "raft_node.h"
#include "log_utils.h"
#include <iostream>
#include <future>
#include <vector>

Heartbeat::Heartbeat(RaftNode &node)
    : node_(node), is_active_(false) {}

void Heartbeat::SendHeartbeat()
{
    if (!ShouldSendHeartbeat())
    {
        return;
    }

    int current_term;
    int leader_id;
    int commit_index;

    {
        std::lock_guard<std::mutex> lock(node_.GetMutex());
        current_term = node_.GetNodeArgs().currentTerm;
        leader_id = node_.GetNodeId();
        commit_index = node_.GetNodeArgs().commitIndex;
    }

    // Prepare heartbeat request (empty AppendEntries)
    configs::AppendEntriesRequest request;
    request.set_term(current_term);
    request.set_leaderid(leader_id);
    request.set_leadercommit(commit_index);

    LOG("Node ", leader_id, " sending heartbeat (term ", current_term, ")");

    // Send heartbeat to all peers asynchronously
    const auto &peers = node_.GetPeers();
    std::vector<std::future<void>> futures;

    for (const auto &pair : peers)
    {
        const std::string &peer_id = pair.first;
        raft::RaftService::Stub *stub = pair.second.get();

        futures.push_back(std::async(std::launch::async,
                                     [this, stub, request, peer_id]()
                                     {
                                         grpc::ClientContext context;
                                         configs::AppendEntriesResponse response;

                                         // Set timeout
                                         std::chrono::system_clock::time_point deadline =
                                             std::chrono::system_clock::now() + std::chrono::milliseconds(50);
                                         context.set_deadline(deadline);

                                         grpc::Status status = stub->HeartSend(&context, request, &response);

                                         if (status.ok())
                                         {
                                             ProcessHeartbeatResponse(response, peer_id);
                                         }
                                         else
                                         {
                                             LOG("Heartbeat to ", peer_id, " failed: ", status.error_message());
                                         }
                                     }));
    }

    // Wait for all heartbeats to complete (non-blocking)
    // In production, you might want to track these futures differently
}

grpc::Status Heartbeat::HandleHeartbeat(
    grpc::ServerContext *context,
    const configs::AppendEntriesRequest *request,
    configs::AppendEntriesResponse *response)
{

    std::lock_guard<std::mutex> lock(node_.GetMutex());

    auto &node_args = node_.GetNodeArgs();

    response->set_term(node_args.currentTerm);
    response->set_success(false);

    // Reply false if term < currentTerm
    if (request->term() < node_args.currentTerm)
    {
        return grpc::Status::OK;
    }

    // If RPC request contains term T > currentTerm: convert to follower
    if (request->term() > node_args.currentTerm)
    {
        node_.BecomeFollower(request->term());
    }

    // Always log and reset election timer when receiving valid heartbeat
    LOG("Node ", node_.GetNodeId(),
        " received heartbeat from leader ", request->leaderid(),
        " in term ", request->term());
    node_.ResetElectionTimer();

    // Update commit index if leader's commit index is higher
    if (request->leadercommit() > node_args.commitIndex)
    {
        int last_log_index = node_.GetLogReplication()->GetLastLogIndex();
        node_args.commitIndex = std::min(request->leadercommit(), last_log_index);
        // Apply committed entries
        node_.GetLogReplication()->ApplyCommittedEntries();
    }

    response->set_success(true);
    response->set_term(node_args.currentTerm);

    return grpc::Status::OK;
}

void Heartbeat::StartHeartbeatTimer()
{
    is_active_.store(true);
}

void Heartbeat::StopHeartbeatTimer()
{
    is_active_.store(false);
}

bool Heartbeat::ShouldSendHeartbeat() const
{
    std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(node_.GetMutex()));
    return node_.GetNodeArgs().state == NodeState::Leader;
}

void Heartbeat::ProcessHeartbeatResponse(
    const configs::AppendEntriesResponse &response,
    const std::string &peerId)
{

    std::lock_guard<std::mutex> lock(node_.GetMutex());

    // If response term is greater than current term, step down
    if (response.term() > node_.GetNodeArgs().currentTerm)
    {
        node_.BecomeFollower(response.term());
        LOG("Node ", node_.GetNodeId(),
            " stepping down, higher term detected: ", response.term());
    }

    // If heartbeat failed, might need to retry or adjust nextIndex
    // This is typically handled by log replication module
}
