# pthreader

Do you have a scientific computing application written in C++ that you want to run multithreaded but don't know how? Then `pthreader` is for you! This particular routine was written with optimization problems in mind, problems of the form

```
minimize f(x;p) with respect to x, given p
```

where the objective `f` is nominally "hard" to calculate, but can be decomposed into some set of embarassingly parallel sub-calculations. For example, minimizing loss functions in statistics: 

```
minimize 1/N \sum_{n=1}^N f(x;p_n) with respect to x, given p_n for all n
```

# Introduction

`pthreader` is a lightweight `C++` class that manages the distribution of work across `Pthreads` (POSIX threads) for you. You just have to specify (a) how many threads, (b) how to "setup" your calculations on any thread, (c) how any thread should evaluate, and (d) how to clean up after yourself. This does require handling "opaque" structures or classes, as setup parameters, in-thread data, and evaluation inputs and outputs are passed as `void *`s. But that's not so bad. 

Surely you can still do bad things with `pthreader`, like issuing conflicting writes to shared locations in memory from different threads. `pthreader` can handle thread safety in signalling that work is available for distribution and that distributed work is done, but not any application-specific thread safety concerns regarding shared memory. A generic package can probably either be lightweight _or_ safe, but not both. 

If each thread sets its own data structures up in memory -- a good thing to do for efficient memory allocation reasons alone -- this is probably not a concern. Nor should it be a concern should different threads want to _read_ the same data, without modifying it. `pthreader` should probably be seen as a helper for doing embarassingly parallel evaluations in which subsets of a calculation don't interact at all. 

# Building

This doesn't build into a library, but you can make an object file with `make pthreader`. The intention is that you just copy `include/pthreader.h` and `src/pthreader.cpp` into your own source tree though. The actual compile commands are simple: you should only need the library flags `-lpthread -lm` when linking, and no special compile-only requirements should exist. 

# Examples

Two dependency-less examples, modeled after Ordinary Least Squares and Binary Logistic Regression, are provided in `examples`. _Obviously_ this is not a real or recommended approach to either problem. The existence of other, very strong, and better solution algorithms is a partial reason why these are suitable examples here. 

You can create them both with `make examples` or separately with `make ols` or `make blr`. The executables are put into `bin`. After building, you can run with commands like
```
./bin/pt_ols 4 10 3 1
```
This would run OLS sum of squares evaluations using a total of 4 threads for data with 10 observations, 3 features and a model including a constant term. Data is generated for this example in the example code. 

There is also an optimization example for OLS, using the [GSL](https://www.gnu.org/software/gsl/doc/html/intro.html) optimizer. If you have GSL, you can try this one too. 

# Contact

[W. Ross Morrow](wrossmorrow@stanford.edu)