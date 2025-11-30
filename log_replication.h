#pragma once

#include "config_args.h"
#include <grpcpp/grpcpp.h>
#include "raft.grpc.pb.h"
#include "configNet.grpc.pb.h"
#include <map>
#include <memory>
#include <mutex>
#include <vector>

class RaftNode;

/**
 * LogReplication module - handles log replication
 * Implements Raft log replication mechanism
 */
class LogReplication
{
public:
    LogReplication(RaftNode &node);
    ~LogReplication() = default;

    /**
     * Append new log entry (only on leader)
     * @param command Command to append
     * @return true if successfully appended and replicated
     */
    bool AppendEntry(const std::string &command);

    /**
     * Handle AppendEntries RPC from leader
     * @param request AppendEntries request
     * @param response AppendEntries response
     * @return gRPC status
     */
    grpc::Status HandleAppendEntries(
        grpc::ServerContext *context,
        const configs::AppendEntriesRequest *request,
        configs::AppendEntriesResponse *response);

    /**
     * Replicate logs to all followers
     * Called by leader periodically or when new entries added
     */
    void ReplicateToFollowers();

    /**
     * Apply committed entries to state machine
     * Updates lastApplied index
     */
    void ApplyCommittedEntries();

    /**
     * Get log entry at index
     * @param index Log index
     * @return LogEntry at index, or nullptr if not found
     */
    const LogEntry *GetLogEntry(int index) const;

    /**
     * Get last log index
     * @return Last log index, or 0 if log is empty
     */
    int GetLastLogIndex() const;

    /**
     * Get last log term
     * @return Last log term, or 0 if log is empty
     */
    int GetLastLogTerm() const;

private:
    /**
     * Check if log contains entry at index with given term
     * @param index Log index
     * @param term Expected term
     * @return true if log contains matching entry
     */
    bool CheckLogConsistency(int index, int term);

    /**
     * Delete entries from index onwards (conflict resolution)
     * @param fromIndex Starting index to delete
     */
    void DeleteEntriesFrom(int fromIndex);

    /**
     * Append entries to log
     * @param entries Entries to append
     */
    void AppendEntries(const std::vector<LogEntry> &entries);

    /**
     * Update commit index based on match index
     * Called by leader after successful replication
     */
    void UpdateCommitIndex();

    /**
     * Send AppendEntries RPC to a specific follower
     * @param stub gRPC stub for follower
     * @param peerId Follower's ID
     * @return AppendEntries response
     */
    configs::AppendEntriesResponse SendAppendEntries(
        raft::RaftService::Stub *stub,
        const std::string &peerId);

    RaftNode &node_;
    std::mutex mutex_;
};
