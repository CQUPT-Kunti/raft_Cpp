#include "raft_node.h"
#include <iostream>
#include <chrono>
#include <random>
#include <thread>

// RaftNode implementation
RaftNode::RaftNode(const NetArgs &net_args, int node_id,
                   const std::vector<NetArgs> &cluster)
    : node_id_(node_id),
      net_args_(net_args),
      cluster_(cluster),
      vote_count_(0),
      running_(false),
      election_timer_reset_(false),
      election_timer_active_(true),
      heartbeat_timer_active_(false)
{

    // Initialize random number generator
    rng_.seed(std::random_device{}() + node_id);

    // Initialize modules
    election_ = std::make_unique<Election>(*this);
    heartbeat_ = std::make_unique<Heartbeat>(*this);
    log_replication_ = std::make_unique<LogReplication>(*this);
}

RaftNode::~RaftNode()
{
    Stop();
}

void RaftNode::Start()
{
    running_.store(true);

    // Initialize gRPC server
    InitializeServer();

    // Initialize peer connections
    InitializePeers();

    // Start election timer thread
    election_timer_thread_ = std::thread(&RaftNode::ElectionTimerRoutine, this);

    // Start heartbeat timer thread
    heartbeat_timer_thread_ = std::thread(&RaftNode::HeartbeatTimerRoutine, this);

    LOG("Node ", node_id_, " started at ", net_args_.ip, ":", net_args_.port);
}

void RaftNode::Stop()
{
    running_.store(false);

    // Stop timers
    election_timer_active_.store(false);
    heartbeat_timer_active_.store(false);

    // Notify timer threads
    election_timer_cv_.notify_all();
    heartbeat_timer_cv_.notify_all();

    if (server_)
    {
        server_->Shutdown();
    }

    if (server_thread_.joinable())
    {
        server_thread_.join();
    }

    if (election_timer_thread_.joinable())
    {
        election_timer_thread_.join();
    }

    if (heartbeat_timer_thread_.joinable())
    {
        heartbeat_timer_thread_.join();
    }

    LOG("Node ", node_id_, " stopped");
}

int RaftNode::GetCurrentTerm() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return node_args_.currentTerm;
}

NodeState RaftNode::GetState() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return node_args_.state;
}

raft::RaftService::Stub *RaftNode::GetPeerStub(const std::string &peer_id)
{
    auto it = peers_.find(peer_id);
    if (it != peers_.end())
    {
        return it->second.get();
    }
    return nullptr;
}

void RaftNode::BecomeFollower(int term)
{
    node_args_.state = NodeState::Follower;
    node_args_.currentTerm = term;
    node_args_.votedFor = -1;

    // Stop heartbeat, start election timer
    StopHeartbeatTimer();
    StartElectionTimer();

    LOG("Node ", node_id_, " became Follower in term ", term);
}

void RaftNode::BecomeCandidate()
{
    node_args_.state = NodeState::Candidate;
    node_args_.currentTerm++;
    node_args_.votedFor = node_id_;

    // Election timer continues running for candidates

    LOG("Node ", node_id_, " became Candidate in term ", node_args_.currentTerm);
}

void RaftNode::BecomeLeader()
{
    node_args_.state = NodeState::Leader;

    // Initialize leader state
    int last_log_index = log_replication_->GetLastLogIndex();
    node_args_.nextIndex.clear();
    node_args_.matchIndex.clear();

    for (size_t i = 0; i < cluster_.size(); i++)
    {
        node_args_.nextIndex.push_back(last_log_index + 1);
        node_args_.matchIndex.push_back(0);
    }

    // Stop election timer, start heartbeat timer
    StopElectionTimer();
    StartHeartbeatTimer();

    LOG("Node ", node_id_, " became Leader in term ", node_args_.currentTerm);
}

void RaftNode::IncrementVoteCount()
{
    vote_count_.fetch_add(1);
}

void RaftNode::ResetElectionTimer()
{
    election_timer_reset_.store(true);
    election_timer_cv_.notify_one();
}

void RaftNode::StopElectionTimer()
{
    election_timer_active_.store(false);
    election_timer_cv_.notify_one();
}

void RaftNode::StartElectionTimer()
{
    election_timer_active_.store(true);
    election_timer_reset_.store(true);
    election_timer_cv_.notify_one();
}

void RaftNode::StopHeartbeatTimer()
{
    heartbeat_timer_active_.store(false);
    heartbeat_timer_cv_.notify_one();
}

void RaftNode::StartHeartbeatTimer()
{
    heartbeat_timer_active_.store(true);
    heartbeat_timer_cv_.notify_one();
}

void RaftNode::InitializeServer()
{
    std::string server_address = net_args_.ip + ":" + net_args_.port;

    RaftServiceImpl *service = new RaftServiceImpl(*this);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(service);

    server_ = builder.BuildAndStart();

    if (server_)
    {
        LOG("Node ", node_id_, " listening on ", server_address);

        // Start server in separate thread
        server_thread_ = std::thread([this]()
                                     { server_->Wait(); });
    }
    else
    {
        std::cerr << "Failed to start server on " << server_address << std::endl;
    }
}

void RaftNode::InitializePeers()
{
    for (const auto &peer : cluster_)
    {
        // Skip self
        if (peer.ip == net_args_.ip && peer.port == net_args_.port)
        {
            continue;
        }

        std::string target = peer.ip + ":" + peer.port;
        peers_[peer.port] = raft::RaftService::NewStub(
            grpc::CreateChannel(target, grpc::InsecureChannelCredentials()));

        LOG("Node ", node_id_, " connected to peer at ", target);
    }
}

int RaftNode::GenerateElectionTimeout()
{
    std::uniform_int_distribution<> dist(150, 300);
    return dist(rng_);
}

void RaftNode::ElectionTimerRoutine()
{
    while (running_.load())
    {
        std::unique_lock<std::mutex> lock(election_timer_mutex_);

        // Check if election timer is active
        if (!election_timer_active_.load())
        {
            // Wait until activated
            election_timer_cv_.wait(lock, [this]()
                                    { return election_timer_active_.load() || !running_.load(); });

            if (!running_.load())
            {
                break;
            }
        }

        // Generate random timeout
        int timeout_ms = GenerateElectionTimeout();
        election_timer_reset_.store(false);

        // Wait for timeout or reset
        bool timeout = election_timer_cv_.wait_for(
            lock,
            std::chrono::milliseconds(timeout_ms),
            [this]()
            {
                return election_timer_reset_.load() ||
                       !election_timer_active_.load() ||
                       !running_.load();
            });

        if (!running_.load())
        {
            break;
        }

        // If not reset and still active, trigger election
        if (!election_timer_reset_.load() && election_timer_active_.load())
        {
            NodeState current_state = GetState();

            // Only non-leaders can start elections
            if (current_state != NodeState::Leader)
            {
                LOG("Node ", node_id_, " election timeout");
                election_->StartElection();
            }
        }
    }
}

void RaftNode::HeartbeatTimerRoutine()
{
    const int HEARTBEAT_INTERVAL_MS = 50;

    while (running_.load())
    {
        std::unique_lock<std::mutex> lock(heartbeat_timer_mutex_);

        // Check if heartbeat timer is active
        if (!heartbeat_timer_active_.load())
        {
            // Wait until activated
            heartbeat_timer_cv_.wait(lock, [this]()
                                     { return heartbeat_timer_active_.load() || !running_.load(); });

            if (!running_.load())
            {
                break;
            }
        }

        // Wait for heartbeat interval
        heartbeat_timer_cv_.wait_for(
            lock,
            std::chrono::milliseconds(HEARTBEAT_INTERVAL_MS));

        if (!running_.load())
        {
            break;
        }

        // If still active and leader, send heartbeat
        if (heartbeat_timer_active_.load())
        {
            NodeState current_state = GetState();

            if (current_state == NodeState::Leader)
            {
                heartbeat_->SendHeartbeat();
            }
        }
    }
}

// RaftServiceImpl implementation
RaftServiceImpl::RaftServiceImpl(RaftNode &node) : node_(node) {}

grpc::Status RaftServiceImpl::RequestVote(
    grpc::ServerContext *context,
    const configs::RequestVoteRequest *request,
    configs::RequestVoteResponse *response)
{

    return node_.GetElection()->HandleVoteRequest(context, request, response);
}

grpc::Status RaftServiceImpl::HeartSend(
    grpc::ServerContext *context,
    const configs::AppendEntriesRequest *request,
    configs::AppendEntriesResponse *response)
{

    // Check if this is a heartbeat or log replication
    if (request->entries_size() == 0)
    {
        return node_.GetHeartbeat()->HandleHeartbeat(context, request, response);
    }
    else
    {
        return node_.GetLogReplication()->HandleAppendEntries(context, request, response);
    }
}
