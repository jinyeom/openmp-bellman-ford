#include <iostream>
#include <string>
#include <cstring>
#include <limits>
#include <fstream>
#include <sstream>
#include <atomic>

#include <time.h>
#include <pthread.h>

#include "ittnotify.h" // for VTune

#include "../include/graph.h"

#define MAX_THREADS 512
#define BUFF_SIZE 100000
#define INF std::numeric_limits<int>::max()

int num_threads = 0;
pthread_t handles[MAX_THREADS];
int short_names[MAX_THREADS];

// initialize the graph in main
graph g;

// NOTE: declare the global array with a buffer; if the argument graph has more nodes than the
// buffer size, reallocate the array with the argument number of nodes.
std::atomic<graph::edge_data_t>* distance = new std::atomic<graph::edge_data_t>[BUFF_SIZE];

void Usage(char *prog) {
    std::cout << "usage: " << prog << " -f filename -s src_id [-p num_threads]" << std::endl;
}

void UpdateDistance(const graph::node_t& u, const graph::node_t& v, const graph::edge_data_t& w) {
    bool done = false;
    while (!done) {
        auto dist_old = distance[v].load();
        auto distu = distance[u].load();
        auto dist_new = distu + w;
        if (distu != INF && dist_new < dist_old) {
            done = distance[v].compare_exchange_weak(dist_old, dist_new);
        } else {
            done = true;
        }
    }
}

void* Relax(void *thread_id_ptr) {
    graph::node_t tid = *(graph::node_t*)thread_id_ptr;
    for (graph::node_t u = tid; u < g.end(); u += num_threads) {
    	for (graph::edge_t e = g.edge_begin(u); e < g.edge_end(u); e++) {
    	    graph::node_t v = g.get_edge_dst(e);
    	    graph::edge_data_t w = g.get_edge_data(e);

    	    // ---------------- critical section ----------------

            UpdateDistance(u, v, w);

    	    // --------------------------------------------------
    	}
    }
}

int main(int argc, char* argv[]) {
    __itt_pause();
    
    std::string filename;               // graph file name
    graph::node_t src;                  // source node ID
    bool parallel = false;              // run in parallel if true
    std::ofstream f;                    // result text file
    std::ostringstream result_filename; // result text file name

    uint64_t execTime;                  // time in nanoseconds
    struct timespec tick, tock;         // for measuring runtime

    pthread_attr_t attr;
    pthread_attr_init(&attr);

    // parse program arguments
    for (int i = 0; i < argc; ++i) {
        if (!strcmp(argv[i], "-f")) {
            // graph file name
            filename = argv[++i];
        } else if (!strcmp(argv[i], "-s")) {
            src = (graph::node_t)std::stoi(argv[++i]);
        } else if (!strcmp(argv[i], "-p")) {
            parallel = true;
            num_threads = std::stoi(argv[++i]);
        }
    }
    if (filename.empty() || (parallel && num_threads == 0)) {
        Usage(argv[0]);
        return -1;
    }
    if (!g.construct_from_dimacs(filename)) {
        return -1;
    }

    // initialize the graph
    int num_nodes = (int)g.size_nodes();
    if (num_nodes > BUFF_SIZE) {
        delete [] distance;
        distance = new std::atomic<graph::edge_data_t>[num_nodes];
    }

    for (int i = 0; i < num_nodes; ++i) {
        distance[i] = INF;
    }
    distance[src - 1] = 0;

    // repeat relaxation
    if (parallel) {
        std::cout << "solving SSSP from node " << (int)src << " via parallel Bellman-Ford algorithm..." << std::endl;
        // main computation for SSSP; measure runtime here
 	clock_gettime(CLOCK_MONOTONIC_RAW, &tick);
	// ---------------- experiment below ----------------
        __itt_resume(); // for VTune

        // this loop has to stay serial
        for (int i = 0; i < num_threads; ++i) {
            // create threads 0, 1, 2, ..., numThreads (round robin)
            short_names[i] = i;
            pthread_create(&handles[i], &attr, Relax, &short_names[i]);
        }
        // join with threads when they're done
        for (int i = 0; i < num_threads; ++i) {
            pthread_join(handles[i], NULL);
        }

        __itt_pause();
        // --------------------------------------------------
        clock_gettime(CLOCK_MONOTONIC_RAW, &tock);
        execTime = 1000000000 * (tock.tv_sec - tick.tv_sec) + tock.tv_nsec - tick.tv_nsec;
        std::cout << "elapsed process CPU time = " << (long long unsigned int)execTime << " nanoseconds\n";
        result_filename << filename << "_parallel.txt";

        // export result to a text file
        f.open(result_filename.str());
        for (int i = 0; i < num_nodes; ++i) {
            std::string dist_str = std::to_string(distance[i]);
            if (distance[i] == INF) dist_str = "INF";
            f << i + 1 << " " << dist_str << std::endl;
        }
        f.close();
    } else {
        std::cout << "solving SSSP from node " << (int)src << " via serial Bellman-Ford algorithm..." << std::endl;
        // main computation for SSSP; measure runtime here
        clock_gettime(CLOCK_MONOTONIC_RAW, &tick);
        // ---------------- experiment below ----------------

        for (graph::node_t u = g.begin(); u < g.end(); u++) {
            graph::edge_data_t dist = distance[u];
            for (graph::edge_t e = g.edge_begin(u); e < g.edge_end(u); e++) {
                graph::node_t v = g.get_edge_dst(e);
                graph::edge_data_t w = g.get_edge_data(e);
                // take care of cases with infinity
                graph::edge_data_t updated = dist + w;
                if (dist != INF && distance[v] > updated) {
                    distance[v] = updated;
                }
            }
        }

        // --------------------------------------------------
        clock_gettime(CLOCK_MONOTONIC_RAW, &tock);
        execTime = 1000000000 * (tock.tv_sec - tick.tv_sec) + tock.tv_nsec - tick.tv_nsec;
        std::cout << "elapsed process CPU time = " << (long long unsigned int)execTime << " nanoseconds\n";
        result_filename << filename << ".txt";

        // export result to a text file
        f.open(result_filename.str());
        for (int i = 0; i < num_nodes; ++i) {
            std::string dist_str = std::to_string(distance[i]);
            if (distance[i] == INF) dist_str = "INF";
            f << i + 1 << " " << dist_str << std::endl;
        }
        f.close();
    }

    // NOTE: normally, the algorithm checks for negative cycles, but for the sake of saving time,
    // we'll just assume there aren't any, assuming all edges have positive weights.
    return 0;
}
