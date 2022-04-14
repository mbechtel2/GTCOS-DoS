/**
 *
 * Copyright (C) 2012  Heechul Yun <heechul@illinois.edu>
 *               2012  Zheng <zpwu@uwaterloo.ca>
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 */

/* clang -S -mllvm --x86-asm-syntax=intel ./bandwidth.c */

/**************************************************************************
 * Conditional Compilation Options
 **************************************************************************/

/**************************************************************************
 * Included Files
 **************************************************************************/
#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include <limits.h>

#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_TARGET_OPENCL_VERSION 120
#include "cl2.hpp"

#include <vector>
#include <string>
#include <sstream>

/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define CACHE_LINE_SIZE 64	   /* cache Line size is 64 byte */
#ifdef __arm__
#  define DEFAULT_ALLOC_SIZE_KB 4096
#else
#  define DEFAULT_ALLOC_SIZE_KB 16384
#endif

#ifndef MAX_SOURCE_SIZE
#define MAX_SOURCE_SIZE (0x100000)
#endif

#define startScalar (1.0)

/**************************************************************************
 * Public Types
 **************************************************************************/

/**************************************************************************
 * Global Variables
 **************************************************************************/
int g_mem_size = DEFAULT_ALLOC_SIZE_KB * 1024;	   /* memory size */
int *g_mem_ptr = 0;		   /* pointer to allocated memory region */

volatile uint64_t g_nread = 0;	           /* number of bytes read */
volatile unsigned int g_start;		   /* starting time */
int cpuid = 0;

cl::Device device;
cl::Context context;
cl::CommandQueue queue;
cl::KernelFunctor<cl::Buffer> *write_kernel;
cl::Buffer cl_mem_ptr;

std::string kernels{R"CLC(
  constant TYPE scalar = startScalar;
  kernel void write(global TYPE * restrict a)
  {
    const size_t i = get_global_id(0);
    a[i] = i;
  }
  
  kernel void memwrite(global TYPE * restrict a)
  {
    const size_t i = get_global_id(0);
    ulong vaddr = (ulong)&a[i];
    
    if ( ( ( (((vaddr >> (6)) & 0x1) ^ ((vaddr >> (13)) & 0x1)) != 0 ) && 
                     ( (((vaddr >> (14)) & 0x1) ^ ((vaddr >> (17)) & 0x1)) != 0 ) && 
                     ( (((vaddr >> (15)) & 0x1) ^ ((vaddr >> (18)) & 0x1)) != 0 ) && 
                     ( (((vaddr >> (16)) & 0x1) ^ ((vaddr >> (19)) & 0x1)) != 0 ) ) ) {
        a[i] = i;
    }
  }
)CLC"};

/**************************************************************************
 * Public Functions
 **************************************************************************/
unsigned int get_usecs()
{
	struct timeval         time;
	gettimeofday(&time, NULL);
	return (time.tv_sec * 1000000 +	time.tv_usec);
}

void quit(int param)
{
	float dur_in_sec;
	float bw;
	float dur = get_usecs() - g_start;
	dur_in_sec = (float)dur / 1000000;
	printf("g_nread(bytes read) = %lld\n", (long long)g_nread);
	printf("elapsed = %.2f sec ( %.0f usec )\n", dur_in_sec, dur);
	bw = (float)g_nread / dur_in_sec / 1024 / 1024;
	printf("CPU%d: B/W = %.2f MB/s | ",cpuid, bw);
	printf("CPU%d: average = %.2f ns\n", cpuid, (dur*1000)/(g_nread/CACHE_LINE_SIZE));
	exit(0);
}

void usage(int argc, char *argv[])
{
	printf("Usage: $ %s [<option>]*\n\n", argv[0]);
	printf("-m: memory size in KB. deafult=8192\n");
	printf("-a: access type - read, write. default=read\n");
	printf("-n: addressing pattern - Seq, Row, Bank. default=Seq\n");
	printf("-t: time to run in sec. 0 means indefinite. default=5. \n");
	printf("-c: CPU to run.\n");
	printf("-i: iterations. 0 means intefinite. default=0\n");
	printf("-p: priority\n");
	printf("-l: log label. use together with -f\n");
	printf("-f: log file name\n");
	printf("-h: help\n");
	printf("\nExamples: \n$ bandwidth -m 8192 -a read -t 1 -c 2\n  <- 8MB read for 1 second on CPU 2\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int64_t sum = 0;
	unsigned finish = 5;
	int prio = 0;        
	int num_processors;
	int opt;
	cpu_set_t cmask;
	int iterations = 0;
	int use_hugepage = 0;	
	int i;
	struct sched_param param;
    
	/*
	  get command line options 
	 */
	while ((opt = getopt(argc, argv, "m:a:n:t:c:i:p:r:f:l:xh")) != -1) {
		switch (opt) {
		case 'm': /* set memory size */
			g_mem_size = 1024 * strtol(optarg, NULL, 0);
			break;			
		case 't': /* set time in secs to run */
			finish = strtol(optarg, NULL, 0);
			break;
                case 'x':
			use_hugepage = (use_hugepage) ? 0: 1;
			break;
		case 'c': /* set CPU affinity */
			cpuid = strtol(optarg, NULL, 0);
			num_processors = sysconf(_SC_NPROCESSORS_CONF);
			CPU_ZERO(&cmask);
			CPU_SET(cpuid % num_processors, &cmask);
			if (sched_setaffinity(0, num_processors, &cmask) < 0)
				perror("error");
			else
				fprintf(stderr, "assigned to cpu %d\n", cpuid);
			break;

		case 'r':
			prio = strtol(optarg, NULL, 0);
			param.sched_priority = prio; /* 1(low)- 99(high) for SCHED_FIFO or SCHED_RR
						        0 for SCHED_OTHER or SCHED_BATCH */
			if(sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
				perror("sched_setscheduler failed");
			}
			break;
		case 'p': /* set priority */
			prio = strtol(optarg, NULL, 0);
			if (setpriority(PRIO_PROCESS, 0, prio) < 0)
				perror("error");
			else
				fprintf(stderr, "assigned priority %d\n", prio);
			break;
		case 'i': /* iterations */
			iterations = strtol(optarg, NULL, 0);
			break;
		case 'h': 
			usage(argc, argv);
			break;
		}
	}

	/* print experiment info before starting */
	printf("memsize=%d KB, type=%s, cpuid=%d\n", g_mem_size/1024, "write", cpuid);
	printf("stop at %d\n", finish);

	/* set signals to terminate once time has been reached */
	signal(SIGINT, &quit);
	if (finish > 0) {
		signal(SIGALRM, &quit);
		alarm(finish);
	}
	
	// Get GPU Device
	std::vector<cl::Device> devices;
	cl::Device device;
	std::vector<cl::Platform> platforms;
	cl::Platform::get(&platforms);
	for (unsigned i = 0; i < platforms.size(); i++)
	{
		std::vector<cl::Device> plat_devices;
		platforms[i].getDevices(CL_DEVICE_TYPE_ALL, &plat_devices);
		devices.insert(devices.end(), plat_devices.begin(), plat_devices.end());
	}
	device = devices[0];
	
    // Create GPU kernel
	context = cl::Context(device);
	queue = cl::CommandQueue(context);
	cl::Program program(context, kernels);
    std::ostringstream args;
    args << "-DstartScalar=" << startScalar << " ";
    args << "-DTYPE=float";
    program.build(args.str().c_str());
	write_kernel = new cl::KernelFunctor<cl::Buffer>(program, "write");
	cl_mem_ptr = cl::Buffer(context, CL_MEM_READ_WRITE, g_mem_size* sizeof(float));

    /*
	 * actual memory access
	 */
    float size = g_mem_size * sizeof(float);
	g_start = get_usecs();
	for (i=0;; i++) {
        // Run the kernel
		(*write_kernel)(cl::EnqueueArgs(queue, cl::NDRange(g_mem_size)), cl_mem_ptr); 
        queue.finish();
		g_nread += size;

		if (iterations > 0 && i+1 >= iterations)
			break;
	}
	
	printf("total sum = %ld\n", (long)sum);
	quit(0);
	return 0;
}

