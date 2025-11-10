#include "raft.h"
#include <future>
int main()
{
    std::vector<netArgs> vec;
    struct netArgs arg1, arg2, arg3;
    arg1 = {"127.0.0.1", "8080"};
    arg2 = {"127.0.0.1", "8081"};
    arg3 = {"127.0.0.1", "8082"};

    vec.push_back(arg1);
    vec.push_back(arg2);
    vec.push_back(arg3);
    int id = 0;

    std::vector<std::future<void>> futures;
    for (auto &arg : vec)
    {
        futures.push_back(std::async(std::launch::async, [arg, id, vec]()
                                     { RaftNode node(arg, id, vec); 
                                     node.StartService(); }));
        id++;
    }

    for (auto &fut : futures)
    {
        fut.get();
    }
    return 0;
}