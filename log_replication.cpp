#include "log_replication.h"
#include "raft_node.h"
#include "log_utils.h"
#include <iostream>
#include <algorithm>

LogReplication::LogReplication(RaftNode &node) : node_(node) {}

bool LogReplication::AppendEntry(const std::string &command)
{
    std::lock_guard<std::mutex> lock(node_.GetMutex());

    // Only leader can append entries
    if (node_.GetNodeArgs().state != NodeState::Leader)
    {
        LOG("Only leader can append entries");
        return false;
    }

    // Create new log entry
    LogEntry entry;
    entry.term = node_.GetNodeArgs().currentTerm;
    entry.index = GetLastLogIndex() + 1;
    entry.command = command;

    // Append to local log
    node_.GetNodeArgs().log.push_back(entry);

    LOG("Node ", node_.GetNodeId(),
        " appended entry: index=", entry.index,
        ", term=", entry.term,
        ", command=", command);

    // Replicate to followers
    // Note: In a real implementation, this should be done asynchronously
    // and the entry should only be considered committed after replication

    return true;
}

grpc::Status LogReplication::HandleAppendEntries(
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

    // If RPC request contains term T >= currentTerm:
    // convert to follower
    if (request->term() >= node_args.currentTerm)
    {
        node_.BecomeFollower(request->term());
    }

    // If this is a heartbeat (no entries), handle it as heartbeat
    if (request->entries_size() == 0)
    {
        response->set_success(true);
        response->set_term(node_args.currentTerm);

        // Update commit index
        if (request->leadercommit() > node_args.commitIndex)
        {
            node_args.commitIndex = std::min(request->leadercommit(),
                                             GetLastLogIndex());
            ApplyCommittedEntries();
        }

        return grpc::Status::OK;
    }

    // Handle log replication
    // Convert protobuf entries to LogEntry
    std::vector<LogEntry> entries;
    for (int i = 0; i < request->entries_size(); i++)
    {
        const auto &proto_entry = request->entries(i);
        LogEntry entry;
        entry.term = proto_entry.term();
        entry.index = proto_entry.index();
        entry.command = proto_entry.command();
        entries.push_back(entry);
    }

    // Append entries
    AppendEntries(entries);

    // Update commit index
    if (request->leadercommit() > node_args.commitIndex)
    {
        node_args.commitIndex = std::min(request->leadercommit(),
                                         GetLastLogIndex());
        ApplyCommittedEntries();
    }

    response->set_success(true);
    response->set_term(node_args.currentTerm);

    LOG("Node ", node_.GetNodeId(),
        " appended ", entries.size(), " entries from leader");

    return grpc::Status::OK;
}

void LogReplication::ReplicateToFollowers()
{
    std::lock_guard<std::mutex> lock(node_.GetMutex());

    // Only leader replicates
    if (node_.GetNodeArgs().state != NodeState::Leader)
    {
        return;
    }

    // For each peer, send AppendEntries with appropriate entries
    const auto &peers = node_.GetPeers();

    for (const auto &pair : peers)
    {
        const std::string &peer_id = pair.first;
        raft::RaftService::Stub *stub = pair.second.get();

        // Send AppendEntries in background
        std::thread([this, stub, peer_id]()
                    {
            auto response = SendAppendEntries(stub, peer_id);
            
            if (response.success()) {
                // Update matchIndex and nextIndex
                // This is simplified; real implementation needs proper tracking
            } })
            .detach();
    }
}

void LogReplication::ApplyCommittedEntries()
{
    auto &node_args = node_.GetNodeArgs();

    // Apply all entries from lastApplied to commitIndex
    while (node_args.lastApplied < node_args.commitIndex)
    {
        node_args.lastApplied++;

        if (node_args.lastApplied <= static_cast<int>(node_args.log.size()))
        {
            const auto &entry = node_args.log[node_args.lastApplied - 1];

            LOG("Node ", node_.GetNodeId(),
                " applying entry: index=", entry.index,
                ", command=", entry.command);

            // Apply to state machine (placeholder)
            // In real implementation, this would execute the command
        }
    }
}

const LogEntry *LogReplication::GetLogEntry(int index) const
{
    std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(node_.GetMutex()));

    if (index <= 0 || index > static_cast<int>(node_.GetNodeArgs().log.size()))
    {
        return nullptr;
    }

    return &node_.GetNodeArgs().log[index - 1];
}

int LogReplication::GetLastLogIndex() const
{
    const auto &log = node_.GetNodeArgs().log;
    return log.empty() ? 0 : log.back().index;
}

int LogReplication::GetLastLogTerm() const
{
    const auto &log = node_.GetNodeArgs().log;
    return log.empty() ? 0 : log.back().term;
}

bool LogReplication::CheckLogConsistency(int index, int term)
{
    if (index <= 0)
    {
        return true;
    }

    const LogEntry *entry = GetLogEntry(index);
    if (entry == nullptr)
    {
        return false;
    }

    return entry->term == term;
}

void LogReplication::DeleteEntriesFrom(int fromIndex)
{
    auto &log = node_.GetNodeArgs().log;

    if (fromIndex <= 0 || fromIndex > static_cast<int>(log.size()))
    {
        return;
    }

    log.erase(log.begin() + fromIndex - 1, log.end());

    LOG("Node ", node_.GetNodeId(),
        " deleted entries from index ", fromIndex);
}

void LogReplication::AppendEntries(const std::vector<LogEntry> &entries)
{
    auto &log = node_.GetNodeArgs().log;

    for (const auto &entry : entries)
    {
        // Check if we need to delete conflicting entries
        if (entry.index <= static_cast<int>(log.size()))
        {
            const auto &existing = log[entry.index - 1];
            if (existing.term != entry.term)
            {
                // Delete conflicting entry and all that follow
                DeleteEntriesFrom(entry.index);
            }
        }

        // Append new entry
        if (entry.index > static_cast<int>(log.size()))
        {
            log.push_back(entry);
        }
    }
}

void LogReplication::UpdateCommitIndex()
{
    auto &node_args = node_.GetNodeArgs();

    // Only leader updates commit index
    if (node_args.state != NodeState::Leader)
    {
        return;
    }

    // Find highest N such that majority of matchIndex[i] >= N
    // and log[N].term == currentTerm

    // This is a simplified implementation
    // Real implementation needs to track matchIndex for each peer

    int last_log_index = GetLastLogIndex();

    for (int n = last_log_index; n > node_args.commitIndex; n--)
    {
        const LogEntry *entry = GetLogEntry(n);
        if (entry && entry->term == node_args.currentTerm)
        {
            // In a real implementation, check if majority has replicated
            // For now, we'll commit immediately (simplified)
            node_args.commitIndex = n;
            ApplyCommittedEntries();
            break;
        }
    }
}

configs::AppendEntriesResponse LogReplication::SendAppendEntries(
    raft::RaftService::Stub *stub,
    const std::string &peerId)
{

    configs::AppendEntriesResponse response;
    grpc::ClientContext context;

    // Prepare AppendEntries request
    configs::AppendEntriesRequest request;

    {
        std::lock_guard<std::mutex> lock(node_.GetMutex());
        auto &node_args = node_.GetNodeArgs();

        request.set_term(node_args.currentTerm);
        request.set_leaderid(node_.GetNodeId());
        request.set_leadercommit(node_args.commitIndex);

        // Add log entries (simplified - send all for now)
        for (const auto &entry : node_args.log)
        {
            auto *proto_entry = request.add_entries();
            proto_entry->set_term(entry.term);
            proto_entry->set_index(entry.index);
            proto_entry->set_command(entry.command);
        }
    }

    // Set timeout
    std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(100);
    context.set_deadline(deadline);

    grpc::Status status = stub->HeartSend(&context, request, &response);

    if (!status.ok())
    {
        LOG("AppendEntries RPC to ", peerId, " failed: ", status.error_message());
        response.set_term(0);
        response.set_success(false);
    }

    return response;
}
