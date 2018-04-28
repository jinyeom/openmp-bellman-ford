import re
import subprocess
import numpy as np
from matplotlib import pyplot as plt

def plot_nthreads_time(prog, filename, src, title, show=False, save=False):
    # plot the naive pthread implemenation of computation of pi with 1, 2, 4, 8 threads
    print("Plotting runtimes and speedups of {} with 1, 2, 4, 8 threads...".format(prog))
    runtimes = []
    speedups = []

    print("Running {} on {} in serial...".format(prog, filename), flush=True, end="")
    result = subprocess.run([prog, "-f", filename, "-s", src], stdout=subprocess.PIPE).stdout.decode("utf-8")
    print("\x1B[32mdone\x1B[0m")
    serial_runtime = float(re.search(r"elapsed process CPU time = (.*) nanoseconds", result).group(1))

    n_threads = [1, 2, 4, 8]
    for n in n_threads:
        print("Running {} on {} with {} thread(s)...".format(prog, filename, n), flush=True, end="")
        result = subprocess.run([prog, "-f", filename, "-s", src, "-p", str(n)], stdout=subprocess.PIPE).stdout.decode("utf-8")
        print("\x1B[32mdone\x1B[0m")
        # first line of the result should have the runtime
        parallel_runtime = float(re.search(r"elapsed process CPU time = (.*) nanoseconds", result).group(1))
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
    if show:
        print("Displaying the plot...")
        plt.show()
    if save:
        print("Saving plot figure...", flush=True, end="")
        plt.savefig("{}_runtime.png".format(title))
        print("\x1B[32mdone\x1B[0m")
        plt.close()
   
    # plot speedups
    plt.figure()
    plt.title(title)
    plt.xlabel("number of threads")
    plt.ylabel("speedup")
    plt.plot(n_threads, speedups, marker="o")
    if show:
        print("Displaying the plot...")
        plt.show()
    if save:
        print("Saving plot figure...", flush=True, end="")
        plt.savefig("{}_speedup.png".format(title))
        print("\x1B[32mdone\x1B[0m")
        plt.close()

plot_nthreads_time("./bellman-ford", "/tmp/jinyeom/rmat15.dimacs", "1", "rmat15", save=True)
plot_nthreads_time("./bellman-ford", "/tmp/jinyeom/rmat22.dimacs", "1", "rmat22", save=True)
plot_nthreads_time("./bellman-ford", "/tmp/jinyeom/roadNY.dimacs", "140961", "roadNY", save=True)
plot_nthreads_time("./bellman-ford", "/tmp/jinyeom/roadFLA.dimacs", "316607", "roadFLA", save=True)
