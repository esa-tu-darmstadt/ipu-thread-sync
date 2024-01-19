# Worker thread synchronization on IPUs

This library provides a set of functions for spawning and synchronizing worker threads from supervisor threads on IPUs.

## Mechanism
The `ipu::startOnAllWorkers` function uses the `runall` instruction to start all worker threads with the `ipu::thread::workerThreadEntryPoint` function. This function has the `colossus_vertex` attribute attached, which causes the compiler to generate a function prologue that sets up the stack pointer. Both functions are templated with the member function passed by the user. The entry function then retrieves the vertex base pointer and the worker thread id and calls the provided member function with the `threadId` as argument. 

## Restrictions
Poplar does not yet consider our worker function when calculating the amount of stack space required for worker threads.

## Usage
Add the include directory to your include path:
```bash
popc -I/path/to/libfvm/libs/ipu-thread-sync/include ...
```

Use the `ipu::startOnAllWorkers` and `ipu::syncAllWorkers` functions to start and synchronize worker threads from a supervisor thread:

```cpp
#include <print.h>
#include <poplar/Vertex.hpp>
#include "ipu-thread-sync/ipu-thread-sync.hpp"

using namespace poplar;

class MyVertex : public SupervisorVertex {
 public:
  Output<Vector<float>> output;

  SUPERVISOR_FUNC bool compute() {
    printf("Hello from supervisor\n");

    // Start function foo on all 6 worker threads
    ipu::startOnAllWorkers<MyVertex, &MyVertex::foo>(this);

    // Wait for all 6 worker threads to finish
    ipu::syncAllWorkers();

    return true;
  }

  WORKER_FUNC bool foo(unsigned threadId) {
    printf("Hello from worker %d\n", threadId);
    output[threadId] = threadId * 42.0f;
    return true;
  }
};
```

For debugging, consider using the `ipu::trap()` function to trigger a patched breakpoint exception from worker or supervisor threads.