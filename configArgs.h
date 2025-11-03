#pragma once
#include <string>
#include <vector>

struct netArgs
{
    std::string ip;
    std::string port;

    bool operator==(const netArgs &other) const
    {
        return ip == other.ip && port == other.port;
    }
};

enum class NodeState
{
    Follower,
    Candidate,
    Leader,
    Dead
};

struct logEntity
{
    int term;
    std::string command;
    int index;
};

struct RequestVoteArgs
{
    int term;         // 候选人的任期号
    int candidateId;  // 候选人ID
    int lastLogIndex; // 候选人最后日志条目的索引值
    int lastLogTerm;  // 候选人最后日志条目的任期号
};

struct RequestVoteReply
{
    int term;         // 当前任期号，用于候选人更新自己
    bool voteGranted; // 候选人赢得了此张选票时为真
};

struct NodeArgs
{
    int currentTerm;
    int votedFor; // 当前任期投票给谁，-1表示未投票
    std::vector<logEntity> log;
    // 易失性状态
    int commitIndex;
    int lastApplied;
    // leader特有
    std::vector<int> nextIndex;
    std::vector<int> matchIndex;

    NodeState state;
};
