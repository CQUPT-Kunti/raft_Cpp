#pragma once

#include "config_args.h"
#include <grpcpp/grpcpp.h>
#include "raft.grpc.pb.h"
#include "configNet.grpc.pb.h"
#include <map>
#include <memory>
#include <mutex>
#include <functional>

class RaftNode;

/**
 * Election module - handles leader election process
 * Implements Raft leader election algorithm
 */
class Election
{
public:
    Election(RaftNode &node);
    ~Election() = default;

    /**
     * Start election process
     * - Convert to Candidate state
     * - Increment current term
     * - Vote for self
     * - Request votes from other nodes
     * @return true if elected as leader, false otherwise
     */
    bool StartElection();

    /**
     * Handle incoming vote request from candidate
     * @param request Vote request from candidate
     * @param response Vote response to send back
     * @return gRPC status
     */
    grpc::Status HandleVoteRequest(
        grpc::ServerContext *context,
        const configs::RequestVoteRequest *request,
        configs::RequestVoteResponse *response);

    /**
     * Reset election timer
     * Called when receiving valid heartbeat from leader
     */
    void ResetElectionTimer();

private:
    /**
     * Check if candidate's log is at least as up-to-date as receiver's log
     * @param lastLogTerm Candidate's last log term
     * @param lastLogIndex Candidate's last log index
     * @return true if candidate's log is up-to-date
     */
    bool IsLogUpToDate(int lastLogTerm, int lastLogIndex);

    /**
     * Send vote request to a peer
     * @param stub gRPC stub for the peer
     * @param request Vote request
     * @return Vote response
     */
    configs::RequestVoteResponse SendVoteRequest(
        raft::RaftService::Stub *stub,
        const configs::RequestVoteRequest &request);

    RaftNode &node_;
    std::mutex mutex_;
};
