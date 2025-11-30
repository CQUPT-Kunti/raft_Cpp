#include "raft_node.h"
#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <signal.h>

std::vector<std::unique_ptr<RaftNode>> nodes;

void signal_handler(int signal)
{
    std::cout << "\nShutting down..." << std::endl;
    for (auto &node : nodes)
    {
        node->Stop();
    }
    exit(0);
}

int main(int argc, char *argv[])
{
    // Register signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Define cluster configuration (3 nodes)
    std::vector<NetArgs> cluster;
    cluster.push_back({"127.0.0.1", "50051"});
    cluster.push_back({"127.0.0.1", "50052"});
    cluster.push_back({"127.0.0.1", "50053"});

    std::cout << "Starting Raft cluster with " << cluster.size() << " nodes..." << std::endl;

    // Create and start nodes in separate threads
    std::vector<std::thread> node_threads;

    for (size_t i = 0; i < cluster.size(); i++)
    {
        node_threads.emplace_back([&cluster, i]()
                                  {
            auto node = std::make_unique<RaftNode>(cluster[i], i, cluster);
            node->Start();
            
            // Keep node running
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            } });
    }

    std::cout << "Cluster started. Press Ctrl+C to stop." << std::endl;

    // Wait for all threads
    for (auto &thread : node_threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    return 0;
}
