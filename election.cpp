#include "election.h"
#include "raft_node.h"
#include "log_utils.h"
#include <iostream>
#include <future>
#include <vector>

Election::Election(RaftNode &node) : node_(node) {}

bool Election::StartElection()
{
    // Convert to candidate and increment term
    {
        std::lock_guard<std::mutex> lock(node_.GetMutex());
        node_.GetNodeArgs().state = NodeState::Candidate;
        node_.GetNodeArgs().currentTerm++;
        node_.GetNodeArgs().votedFor = node_.GetNodeId();
        node_.ResetVoteCount();
        node_.IncrementVoteCount(); // Vote for self
    }

    int current_term;
    int last_log_index;
    int last_log_term;

    {
        std::lock_guard<std::mutex> lock(node_.GetMutex());
        current_term = node_.GetNodeArgs().currentTerm;
        last_log_index = node_.GetLogReplication()->GetLastLogIndex();
        last_log_term = node_.GetLogReplication()->GetLastLogTerm();

        LOG("Node ", node_.GetNodeId(), " starts election for term ", current_term);
    }

    // Prepare vote request
    configs::RequestVoteRequest request;
    request.set_term(current_term);
    request.set_condidateid(node_.GetNodeId());
    request.set_lastlogindex(last_log_index);
    request.set_lastlogterm(last_log_term);

    // Send vote requests to all peers in parallel
    const auto &peers = node_.GetPeers();
    std::vector<std::future<configs::RequestVoteResponse>> futures;

    for (const auto &pair : peers)
    {
        raft::RaftService::Stub *stub = pair.second.get();

        futures.push_back(std::async(std::launch::async,
                                     [this, stub, request]()
                                     {
                                         return SendVoteRequest(stub, request);
                                     }));
    }

    // Calculate majority
    int total_nodes = peers.size() + 1; // Including self
    int majority = total_nodes / 2 + 1;

    // Collect votes
    for (auto &future : futures)
    {
        try
        {
            auto response = future.get();

            // Check if our term is outdated
            if (response.term() > current_term)
            {
                std::lock_guard<std::mutex> lock(node_.GetMutex());
                if (response.term() > node_.GetNodeArgs().currentTerm)
                {
                    node_.BecomeFollower(response.term());
                    return false;
                }
            }

            // Count vote
            if (response.votegranted())
            {
                node_.IncrementVoteCount();
                int votes = node_.GetVoteCount();

                LOG("Node ", node_.GetNodeId(),
                    " received vote (", votes, "/", majority, ")");

                // Check if won election
                if (votes >= majority)
                {
                    std::lock_guard<std::mutex> lock(node_.GetMutex());
                    if (node_.GetNodeArgs().state == NodeState::Candidate)
                    {
                        node_.BecomeLeader();
                        return true;
                    }
                }
            }
        }
        catch (const std::exception &e)
        {
            LOG("Error collecting vote: ", e.what());
        }
    }

    return false;
}

grpc::Status Election::HandleVoteRequest(
    grpc::ServerContext *context,
    const configs::RequestVoteRequest *request,
    configs::RequestVoteResponse *response)
{

    std::lock_guard<std::mutex> lock(node_.GetMutex());

    auto &node_args = node_.GetNodeArgs();

    response->set_term(node_args.currentTerm);
    response->set_votegranted(false);

    // Reply false if term < currentTerm
    if (request->term() < node_args.currentTerm)
    {
        return grpc::Status::OK;
    }

    // If RPC request or response contains term T > currentTerm:
    // set currentTerm = T, convert to follower
    if (request->term() > node_args.currentTerm)
    {
        node_.BecomeFollower(request->term());
    }

    // If votedFor is null or candidateId, and candidate's log is at
    // least as up-to-date as receiver's log, grant vote
    bool can_vote = (node_args.votedFor == -1 ||
                     node_args.votedFor == request->condidateid());

    if (can_vote && IsLogUpToDate(request->lastlogterm(), request->lastlogindex()))
    {
        node_args.votedFor = request->condidateid();
        response->set_votegranted(true);

        LOG("Node ", node_.GetNodeId(),
            " granted vote to ", request->condidateid(),
            " in term ", request->term());
    }

    response->set_term(node_args.currentTerm);
    return grpc::Status::OK;
}

void Election::ResetElectionTimer()
{
    // This would be implemented with timer management
    // For now, it's a placeholder for timer reset logic
}

bool Election::IsLogUpToDate(int lastLogTerm, int lastLogIndex)
{
    int my_last_log_term = node_.GetLogReplication()->GetLastLogTerm();
    int my_last_log_index = node_.GetLogReplication()->GetLastLogIndex();

    // Candidate's log is more up-to-date if:
    // 1. Last log term is higher, or
    // 2. Last log terms are equal and candidate's log is at least as long
    if (lastLogTerm != my_last_log_term)
    {
        return lastLogTerm > my_last_log_term;
    }

    return lastLogIndex >= my_last_log_index;
}

configs::RequestVoteResponse Election::SendVoteRequest(
    raft::RaftService::Stub *stub,
    const configs::RequestVoteRequest &request)
{

    configs::RequestVoteResponse response;
    grpc::ClientContext context;

    // Set timeout for RPC
    std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(100);
    context.set_deadline(deadline);

    grpc::Status status = stub->RequestVote(&context, request, &response);

    if (!status.ok())
    {
        LOG("RequestVote RPC failed: ", status.error_message());
        response.set_term(0);
        response.set_votegranted(false);
    }

    return response;
}
