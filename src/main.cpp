#include <iostream>
#include <string>
#include <cstring>
#include <limits>
#include <fstream>
#include <sstream>
#include <atomic>

#include <time.h>
#include <omp.h>

#include "../include/graph.h"

#define NUM_TRIALS 5
#define BUFF_SIZE 100000
#define INF std::numeric_limits<int>::max()

// NOTE: initialize the graph in main
graph g;

// NOTE: declare the global array with a buffer; if the argument graph has more nodes than the
// buffer size, reallocate the array with the argument number of nodes.
std::atomic<graph::edge_data_t>* distance = new std::atomic<graph::edge_data_t>[BUFF_SIZE];

void Usage(char *prog) {
    std::cout << "usage: " << prog << " -f filename -s src_id [-p num_threads]" << std::endl;
}

// update distance between two nodes given the current weight using a CAS operation
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

int main(int argc, char* argv[]) {
    // __itt_pause();

    std::string filename;               // graph file name
    graph::node_t src;                  // source node ID
    bool parallel = false;              // run in parallel if true
    int num_threads = 0;                // number of threads
    std::string clause;                 // schedule clause (either static or dynamic)
    int chunk_size = 0;                 // chunk size for scheduling

    std::ofstream f;                    // result text file
    std::ostringstream result_filename; // result text file name

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
            clause = argv[++i];
            chunk_size = std::stoi(argv[++i]);
        }
    }
    if (filename.empty() || (parallel && num_threads == 0 && clause.empty() && chunk_size == 0)) {
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

    if (parallel) {
        // OpenMP parallel implementaion
        std::cout << "solving SSSP from node " << (int)src << " via parallel (" << num_threads << ") Bellman-Ford algorithm..." << std::endl;

        double execTime = 0.0; // time in nanoseconds
        double tick, tock; // in seconds

        // set the number of threads for OpenMP
        omp_set_num_threads(num_threads);

        if (clause == "static") {
            for (int t = 0; t < NUM_TRIALS; ++t) {
                #pragma omp parallel
                {
                    #pragma omp master
                    {
                        int nthreads = omp_get_num_threads();
                        if (nthreads != num_threads) {
                            std::cout << "incorrect number of threads: " << nthreads << " != " << num_threads << std::endl;
                        }
                        tick = omp_get_wtime();
                    }
                    // ---------------- experiment below ----------------

                    #pragma omp for schedule(static, chunk_size)
                        for (graph::node_t u = g.begin(); u < g.end(); ++u) {
                            graph::edge_data_t dist = distance[u];
                            for (graph::edge_t e = g.edge_begin(u); e < g.edge_end(u); ++e) {
                                graph::node_t v = g.get_edge_dst(e);
                                graph::edge_data_t w = g.get_edge_data(e);
                                UpdateDistance(u, v, w);
                            }
                        }

                    // --------------------------------------------------
                    #pragma omp master
                    {
                        tock = omp_get_wtime();
                        execTime += 1000000000.0 * (tock - tick);
                    }
                }
            }
        } else if (clause == "dynamic") {
            for (int t = 0; t < NUM_TRIALS; ++t) {
                #pragma omp parallel
                {
                    #pragma omp master
                    {
                        int nthreads = omp_get_num_threads();
                        if (nthreads != num_threads) {
                            std::cout << "incorrect number of threads: " << nthreads << " != " << num_threads << std::endl;
                        }
                        tick = omp_get_wtime();
                    }
                    // ---------------- experiment below ----------------

                    #pragma omp for schedule(dynamic, chunk_size)
                        for (graph::node_t u = g.begin(); u < g.end(); ++u) {
                            graph::edge_data_t dist = distance[u];
                            for (graph::edge_t e = g.edge_begin(u); e < g.edge_end(u); ++e) {
                                graph::node_t v = g.get_edge_dst(e);
                                graph::edge_data_t w = g.get_edge_data(e);
                                UpdateDistance(u, v, w);
                            }
                        }

                    // --------------------------------------------------
                    #pragma omp master
                    {
                        tock = omp_get_wtime();
                        execTime += 1000000000.0 * (tock - tick);
                    }
                }
            }
        }
        execTime = execTime / (double)NUM_TRIALS;
        std::cout << "elapsed process CPU time = " << execTime << " nanoseconds\n";
        result_filename << "parallel_" << filename << ".txt";

        // export result to a text file
        f.open(result_filename.str());
        for (int i = 0; i < num_nodes; ++i) {
            std::string dist_str = std::to_string(distance[i]);
            if (distance[i] == INF) dist_str = "INF";
            f << i + 1 << " " << dist_str << std::endl;
        }
        f.close();

    } else {
        // serial implementation
        std::cout << "Solving SSSP from node " << (int)src << " via serial Bellman-Ford algorithm..." << std::endl;

        double execTime = 0.0; // time in nanoseconds
        struct timespec tick, tock;

        for (int t = 0; t < NUM_TRIALS; ++t) {
            // main computation for SSSP; measure runtime here
            // NOTE: probably should flush cache every run here...
            clock_gettime(CLOCK_MONOTONIC_RAW, &tick);
            // ---------------- experiment below ----------------

            for (graph::node_t u = g.begin(); u < g.end(); ++u) {
                graph::edge_data_t dist = distance[u];
                for (graph::edge_t e = g.edge_begin(u); e < g.edge_end(u); ++e) {
                    graph::node_t v = g.get_edge_dst(e);
                    graph::edge_data_t w = g.get_edge_data(e);
                    UpdateDistance(u, v, w);
                }
            }

            // --------------------------------------------------
            clock_gettime(CLOCK_MONOTONIC_RAW, &tock);
            execTime += 1000000000.0 * (tock.tv_sec - tick.tv_sec) + tock.tv_nsec - tick.tv_nsec;
        }
        execTime = execTime / (double)NUM_TRIALS;
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
