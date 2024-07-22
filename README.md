# for more detail please refer to the Knowledge planet repository maintained by myself
## https://github.com/youngyangyang04/coroutine-lib

# Project Overview
This project is a modification of the Sylar server framework, focusing on the coroutine library. By introducing coroutines, schedulers, and timers as core modules, and utilizing HOOK technology, I have transformed traditional synchronous functions in Linux systems (such as `sleep`, `read`, `write` etc.) into asynchronous versions. This transformation allows maintaining the programming style of synchronous I/O while realizing the efficiency and speed of asynchronous execution.

## Main Modules Introduction

### Coroutine 
 * Uses non-symmetric independent stack coroutines.
 * Supports efficient switching between scheduling coroutines and task coroutines.

### Scheduler
 * Maintains a thread pool and a task queue.
 * Working threads use a FIFO strategy to run coroutine tasks and are responsible for adding epoll-ready file descriptor events tasks and timeout tasks to the queue.

### Timer
 * Manages timers using the min-heap algorithm to optimize the efficiency of obtaining timeout callback functions.

## Key Technologies

 * Thread and synchronization
 * Thread pool 
 * epoll's event-driven model
 * Linux networking programming
 * Generic programming
 * Synchronous and Asynchronous I/O
 * HOOK technology

## Features to Optimize and Extend

### Memory Pool Optimization
Currently, coroutines automatically allocate independent stack space when created and release it upon destruction, involving frequent system calls. Optimizing through memory pool technology can reduce system calls and improve memory usage efficiency.

### Coroutine Nesting Support
Currently, only switching between the main coroutine and child coroutines is supported, without the capability for coroutine nesting. By referencing the design of libco, implement coroutine calling stack, allowing the creation of new coroutine layers within a coroutine.

### Complex Scheduling Algorithms
Introduce scheduling algorithms similar to those used in operating systems, such as priority, response ratio, and time-slicing, to support more complex scheduling strategies and meet the needs of different scenarios.
