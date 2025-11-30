#pragma once
#include <string>
#include <vector>
#include <chrono>

// Network configuration
struct NetArgs
{
    std::string ip;
    std::string port;

    bool operator==(const NetArgs &other) const
    {
        return ip == other.ip && port == other.port;
    }
};

// Node state enumeration
enum class NodeState
{
    Follower,
    Candidate,
    Leader,
    Dead
};

// Log entry structure
struct LogEntry
{
    int term;            // Term when entry was received by leader
    int index;           // Index in the log
    std::string command; // State machine command

    LogEntry() : term(0), index(0), command("") {}
    LogEntry(int t, int idx, const std::string &cmd)
        : term(t), index(idx), command(cmd) {}
};

// Node persistent and volatile state
struct NodeArgs
{
    // Persistent state
    int currentTerm;
    int votedFor; // -1 if none
    std::vector<LogEntry> log;

    // Volatile state on all servers
    int commitIndex;
    int lastApplied;

    // Volatile state on leaders (reinitialized after election)
    std::vector<int> nextIndex;
    std::vector<int> matchIndex;

    NodeState state;

    NodeArgs()
        : currentTerm(0), votedFor(-1), commitIndex(0),
          lastApplied(0), state(NodeState::Follower) {}
};
