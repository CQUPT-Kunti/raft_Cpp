#pragma once
#include <string>

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
