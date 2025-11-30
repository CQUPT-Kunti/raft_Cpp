#pragma once

#include "config_args.h"
#include "election.h"
#include "heartbeat.h"
#include "log_replication.h"
#include "log_utils.h"
#include <grpcpp/grpcpp.h>
#include "raft.grpc.pb.h"
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <random>

// Forward declarations
class Election;
class Heartbeat;
class LogReplication;

/**
 * RaftNode - Main Raft node class
 * Coordinates election, heartbeat, and log replication modules
 */
class RaftNode
{
public:
    RaftNode(const NetArgs &net_args, int node_id, const std::vector<NetArgs> &cluster);
    ~RaftNode();

    /**
     * Start the Raft node service
     * - Starts gRPC server
     * - Initializes timers
     * - Begins Raft protocol
     */
    void Start();

    /**
     * Stop the Raft node service
     */
    void Stop();

    /**
     * Get node ID
     */
    int GetNodeId() const { return node_id_; }

    /**
     * Get current term
     */
    int GetCurrentTerm() const;

    /**
     * Get current state
     */
    NodeState GetState() const;

    /**
     * Get node args (state)
     */
    NodeArgs &GetNodeArgs() { return node_args_; }
    const NodeArgs &GetNodeArgs() const { return node_args_; }

    /**
     * Get network args
     */
    const NetArgs &GetNetArgs() const { return net_args_; }

    /**
     * Get cluster configuration
     */
    const std::vector<NetArgs> &GetCluster() const { return cluster_; }

    /**
     * Get gRPC stub for peer
     */
    raft::RaftService::Stub *GetPeerStub(const std::string &peer_id);

    /**
     * Get all peer stubs
     */
    const std::map<std::string, std::unique_ptr<raft::RaftService::Stub>> &GetPeers() const
    {
        return peers_;
    }

    /**
     * Get mutex for state synchronization
     */
    std::mutex &GetMutex() { return mutex_; }

    /**
     * Convert to follower
     */
    void BecomeFollower(int term);

    /**
     * Convert to candidate
     */
    void BecomeCandidate();

    /**
     * Convert to leader
     */
    void BecomeLeader();

    /**
     * Get election module
     */
    Election *GetElection() { return election_.get(); }

    /**
     * Get heartbeat module
     */
    Heartbeat *GetHeartbeat() { return heartbeat_.get(); }

    /**
     * Get log replication module
     */
    LogReplication *GetLogReplication() { return log_replication_.get(); }

    /**
     * Increment vote count (thread-safe)
     */
    void IncrementVoteCount();

    /**
     * Get vote count
     */
    int GetVoteCount() const { return vote_count_.load(); }

    /**
     * Reset vote count
     */
    void ResetVoteCount() { vote_count_.store(0); }

    /**
     * Reset election timer (called when receiving heartbeat)
     */
    void ResetElectionTimer();

    /**
     * Stop election timer (called when becoming leader)
     */
    void StopElectionTimer();

    /**
     * Start election timer (called when becoming follower/candidate)
     */
    void StartElectionTimer();

    /**
     * Stop heartbeat timer (called when stepping down from leader)
     */
    void StopHeartbeatTimer();

    /**
     * Start heartbeat timer (called when becoming leader)
     */
    void StartHeartbeatTimer();

private:
    /**
     * Initialize gRPC server
     */
    void InitializeServer();

    /**
     * Initialize peer connections
     */
    void InitializePeers();

    /**
     * Election timer thread function
     */
    void ElectionTimerRoutine();

    /**
     * Heartbeat timer thread function
     */
    void HeartbeatTimerRoutine();

    /**
     * Generate random election timeout (150-300ms)
     */
    int GenerateElectionTimeout();

    // Node identification
    int node_id_;
    NetArgs net_args_;
    std::vector<NetArgs> cluster_;

    // Node state
    NodeArgs node_args_;
    std::atomic<int> vote_count_;

    // Modules
    std::unique_ptr<Election> election_;
    std::unique_ptr<Heartbeat> heartbeat_;
    std::unique_ptr<LogReplication> log_replication_;

    // gRPC components
    std::unique_ptr<grpc::Server> server_;
    std::map<std::string, std::unique_ptr<raft::RaftService::Stub>> peers_;

    // Threading
    std::thread server_thread_;
    std::thread election_timer_thread_;
    std::thread heartbeat_timer_thread_;
    std::atomic<bool> running_;

    // Election timer control
    std::mutex election_timer_mutex_;
    std::condition_variable election_timer_cv_;
    std::atomic<bool> election_timer_reset_;
    std::atomic<bool> election_timer_active_;

    // Heartbeat timer control
    std::mutex heartbeat_timer_mutex_;
    std::condition_variable heartbeat_timer_cv_;
    std::atomic<bool> heartbeat_timer_active_;

    // Random number generator for election timeout
    std::mt19937 rng_;

    // Synchronization
    mutable std::mutex mutex_;

    friend class Election;
    friend class Heartbeat;
    friend class LogReplication;
};

/**
 * RaftServiceImpl - gRPC service implementation
 * Handles incoming RPC requests
 */
class RaftServiceImpl final : public raft::RaftService::Service
{
public:
    explicit RaftServiceImpl(RaftNode &node);

    grpc::Status RequestVote(
        grpc::ServerContext *context,
        const configs::RequestVoteRequest *request,
        configs::RequestVoteResponse *response) override;

    grpc::Status HeartSend(
        grpc::ServerContext *context,
        const configs::AppendEntriesRequest *request,
        configs::AppendEntriesResponse *response) override;

private:
    RaftNode &node_;
};
