#pragma once

#include "config_args.h"
#include <grpcpp/grpcpp.h>
#include "raft.grpc.pb.h"
#include "configNet.grpc.pb.h"
#include <map>
#include <memory>
#include <mutex>
#include <atomic>

class RaftNode;

/**
 * Heartbeat module - handles leader heartbeat mechanism
 * Implements periodic heartbeat to maintain leadership
 */
class Heartbeat
{
public:
    Heartbeat(RaftNode &node);
    ~Heartbeat() = default;

    /**
     * Send heartbeat to all followers
     * Only called by leader
     */
    void SendHeartbeat();

    /**
     * Handle incoming heartbeat from leader
     * @param request Heartbeat request from leader
     * @param response Heartbeat response to send back
     * @return gRPC status
     */
    grpc::Status HandleHeartbeat(
        grpc::ServerContext *context,
        const configs::AppendEntriesRequest *request,
        configs::AppendEntriesResponse *response);

    /**
     * Start periodic heartbeat (for leader)
     * Called when node becomes leader
     */
    void StartHeartbeatTimer();

    /**
     * Stop periodic heartbeat
     * Called when leader steps down
     */
    void StopHeartbeatTimer();

    /**
     * Check if should send heartbeat
     * @return true if node is leader
     */
    bool ShouldSendHeartbeat() const;

private:
    /**
     * Process heartbeat response from follower
     * @param response Response from follower
     * @param peerId ID of the follower
     */
    void ProcessHeartbeatResponse(
        const configs::AppendEntriesResponse &response,
        const std::string &peerId);

    RaftNode &node_;
    std::mutex mutex_;
    std::atomic<bool> is_active_;
};
