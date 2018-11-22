# pthreader

Do you have a scientific computing application written in C++ that you want to run multithreaded but don't know how? Then `pthreader` is for you! 

`pthreader` is a lightweight C++ class that manages the distribution of work across `Pthreads` (POSIX threads) for you. You just have to specify (a) how many threads, (b) how to "setup" your calculations on any thread, (c) how any thread should evaluate, and (d) how to clean up after yourself. This does require handling "opaque" structures or classes, as setup parameters, in-thread data, and evaluation inputs and outputs are passed as `void *`s. But that's not so bad. 

Surely you can still do bad things with `pthreader`, like issueing conflicting writes to shared locations in memory from different threads. `pthreader` can handle thread safety in signalling that work is available and work is done, but not any application-specific thread safety concerns. A generic package can probably either be lightweight _or_ safe, but not both. 

If each thread sets its own data structures up in memory -- a good thing to do for efficient memory allocation reasons alone -- this is probably not a concern. Nor is it a concern should different threads want to read the same data. `pthreader` should probably be seen as a helper for doing embarassingly parallel evaluations in which subsets of a calculation don't interact at all. 

# Building

This doesn't build into a library, but you can make an object file with `make pthreader`. The intention is that you just copy `include/pthreader.h` and `src/pthreader.h` into your own source tree though. 

# Examples

Two dependency-less examples, modeled after Ordinary Least Squares and Binary Logistic Regression, are provided in `examples`. You can create them both with `make examples` or separately with `make ols` or `make blr`. The executables are put into `bin`. 

# Contact

[W. Ross Morrow](wrossmorrow@stanford.edu)