import re
import os
import subprocess
import numpy as np
from matplotlib import pyplot as plt

def omp_schedule_plot(prog, filename, src, title, save):
    clause = ["static", "dynamic"]
    chunk_sizes = [1, 8, 32, 128, 512]
    for c in clause:
        for s in chunk_sizes:
            os.environ["OMP_SCHEDULE"] = "{},{}".format(c, s)
            plot_nthreads_time(prog, filename, src, title, c, s, save)

def plot_nthreads_time(prog, filename, src, title, clause, chunk_size, save):
    # plot the naive pthread implemenation of computation of pi with 1, 2, 4, 8 threads
    print("Plotting runtimes and speedups of {} with 1, 2, 4, 8 threads...".format(prog))
    runtimes = []
    speedups = []

    # first get the runtime of serial implementation
    print("Running {} on {} in serial...".format(prog, filename), flush=True, end="")
    serial_runtime = 0.0
    for i in range(5):
        result = subprocess.run([prog, "-f", filename, "-s", src], stdout=subprocess.PIPE).stdout.decode("utf-8")
        serial_runtime += float(re.search(r"elapsed process CPU time = (.*) nanoseconds", result).group(1))
    serial_runtime = serial_runtime / 5.0 # get average time
    print("\x1B[32mdone\x1B[0m")

    n_threads = [1, 2, 4, 8]
    for n in n_threads:
        print("Running {} on {} with {} thread(s) ({}, {})...".format(prog, filename, n, clause, chunk_size), flush=True, end="")
        parallel_runtime = 0.0
        for i in range(5):
            result = subprocess.run([prog, "-f", filename, "-s", src, "-p", str(n)], stdout=subprocess.PIPE).stdout.decode("utf-8")
            parallel_runtime += float(re.search(r"elapsed process CPU time = (.*) nanoseconds", result).group(1))
        parallel_runtime = parallel_runtime / 5.0 # get average time
        print("\x1B[32mdone\x1B[0m")
        
        runtimes.append(parallel_runtime)
        speedups.append(serial_runtime / parallel_runtime)

    # plot runtimes
    plt.figure()
    plt.title(title)
    plt.xlabel("number of threads")
    plt.ylabel("runtime (nanoseconds)")
    plt.axhline(y=serial_runtime, color="r", label="serial")
    plt.plot(n_threads, runtimes, marker="o")
    plt.legend()
    if save:
        print("Saving plot figure...", flush=True, end="")
        plt.savefig("{}_runtime_({},{}).png".format(title, clause, chunk_size))
        print("\x1B[32mdone\x1B[0m")
        plt.close()
   
    # plot speedups
    plt.figure()
    plt.title(title)
    plt.xlabel("number of threads")
    plt.ylabel("speedup")
    plt.plot(n_threads, speedups, marker="o")
    if save:
        print("Saving plot figure...", flush=True, end="")
        plt.savefig("{}_speedup_({},{}).png".format(title, clause, chunk_size))
        print("\x1B[32mdone\x1B[0m")
        plt.close()

omp_schedule_plot("./bellman-ford", "/tmp/jinyeom/rmat15.dimacs", "1", "rmat15", True)
omp_schedule_plot("./bellman-ford", "/tmp/jinyeom/rmat22.dimacs", "1", "rmat22", True)
omp_schedule_plot("./bellman-ford", "/tmp/jinyeom/roadNY.dimacs", "140961", "roadNY", True)
omp_schedule_plot("./bellman-ford", "/tmp/jinyeom/roadFLA.dimacs", "316607", "roadFLA", True)
