# GTCOS-DoS

This repository contains code for recreating the experimental results in our paper "Denial-of-Service Attacks on Shared Resources in Intel's Integrated CPU-GPU Platforms".

Note that we only provide code for the experiments run with a synthetic workload as the victim task.

## Initialize All Benchmarks

From this directory, run the following commands to initialize the IsolBench suite:

    $ git submodule update --init
    $ git submodule update --recursive

Before building the benchmarks, the following file should be copied to the IsolBench/bench directory. This file is a different version of the Memory Aware DoS attack that is tailored specifically for Intel Coffee Lake CPUs.

    $ cp MemAware/attacker4.cpp Isolbench/bench/attacker4.cpp
    
Once copied, build and install the necessary IsolBench benchmarks with:

    $ cd IsolBench/bench
    $ make bandwidth latency-mlp attacker4
    $ sudo cp bandwidth latency-mlp attacker4 /usr/bin
    $ cd ../..
    
Lastly, build and install the GPU DoS attacker in the OCL directory:

    $ cd OCL
    $ make
    $ sudo cp gpuwrite /usr/bin
    $ cd ..
    
## Run the Tests

The tests for the synthetic bandwidth victim can be run with the following script. The script itself takes one command line argument, <use_cache_part>, to determine if CAT/GTCOS partitioning is enabled or disabled. A value of 1 enables CAT/GTCOS while a value of 0 disables them:

    $ sudo ./bw_vs_all.sh <use_cache_part>  # 1 for partitioning, 0 for no partitioning

Likewise, we the following script can be used to run tests with a synthetic latency-mlp victim task, which uses the same command line argument:

    $ sudo ./mlp_vs_all.sh <use_cache_part>  # 1 for partitioning, 0 for no partitioning

Once completed, the figures showing the victim slowdown and LLC missrate results can be found in the figs directory. For the slowdown figures, the y-axis range can be changed to better display the results for some platforms (e.g. Tiger Lake UP3). This can be done by changing the min and max values in the following line from the corresponding .scr files:

    set yrange [0:100] # Change values as necessary
    
The graphs can be redrawn by then running:

    $ cd figs
    $ gnuplot bw-gen.gp # or mlp-gen.gp
    $ cd ..
