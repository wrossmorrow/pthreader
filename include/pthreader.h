
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * 
 * PTHREADER
 * 
 *  	A class to (hopefully) make distribution of generic work easier with pthreads. All you have to
 *		specify is functions to call to setup, evaluate, and clean up. Each of these functions operates
 *		on a "void * data" object: setup defines the "real" object and returns a pointer, and evaluate
 *		and clean up, when called in a thread, will be passed the pointer for what was setup for that 
 * 		thread. (In part this is for generality, but also efficiency: memory allocations are probably
 *		best done by the threads that will use the memory, which this setup-call model allows.)
 * 
 * TEMPLATE FOR USE: 
 * 
 * 		pthreader * P = new pthreader( T );		// create a new object with T threads (total, launches T-1)
 * 		
 *		P->set_setup( my_setup );				// define the per-thread setup function
 *		P->set_evaluation( my_evaluation );		// define the per-thread evaluation function
 *		P->set_cleanup( my_cleanup );			// define the per-thread cleanup function
 * 
 *		P->launch();							// calls the setup function in each thread
 * 		
 *		... 
 *			
 *		P->evaluate();							// do an evaluation on the current values of the data objects
 * 		
 *		... 
 * 
 *		P->close();								// calls the cleanup function in each thread and kills them
 * 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef _PTHREADER_H_
#define _PTHREADER_H_

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * 
 * DEPENDENCIES
 * 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * 
 * 
 * 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// setup functions will be passed the thread number, the number of threads, and a pointer to "initial data", 
// and returns a pointer to thread-specific parameter data. You can store whatever you want behind that pointer, 
// as long as you specify a "cleanup" function that cleans up after yourself. 
typedef void * (*pthreader_setup_fcn)( int , int , void * );

// eval function gets the thread number, the setup-computed parameters, any input data passed to evaluate, and 
// any output we need to fill in
typedef int (*pthreader_eval_fcn)( int , void * , void * , void * );

// other functions will get the thread number and the pointer returned from a previous, in-thread call to setup
typedef void (*pthreader_free_fcn)( int , void ** );

// this datatype holds data needed by the generic thread evaluation
typedef struct pthreader_params {

	int exit;		// exitflag
	int thrd;		// thread identifier
	int nthd;		// number of threads
	int prnt;		// "print", like the verbose flag

	void * init_data; // "shared" or "initial" data passed through, for setup

	pthreader_setup_fcn thread_alloc; 	// setup ("allocate") function handle
	pthreader_eval_fcn thread_eval; 	// evaluate function handle
	pthreader_free_fcn thread_free;		// cleanup ("free") function handle
	void * eval_params; // any parameters to pass through; MUST BE ASSIGNED IN SETUP FUNCTION CALL PROVIDED
	void * eval_in; // data for evaluation, passed through evaluation
	void * eval_out; // data for evaluation results, passed through evaluation

	int * workflag;				// pointer to a flag indicating if there is work to do
	int * statflag;				// pointer to a flag indicating evlauation status
	pthread_mutex_t * worklock; // pointer to a worklock to ensure mutual exclusivity of workflag in shared memory
	pthread_cond_t * cv_work; 	// pointer to a condition variable indicating work to do
	pthread_cond_t * cv_free; 	// pointer to a condition variable indicating work done

	pthread_mutex_t * prntlock; // for verbose printing (have to ensure mutual exclusivity for sensible prints)

} pthreader_params;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * 
 * PTHREADER CLASS
 * 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class pthreader
{

private: 

	int verbose = 0;			// flag to identify if we are being verbose
	int threads_open = 0;		// flag to identify if threads are running

	int n_threads = 1;			// number of threads
	int n_threads_minus_one;	// self-explanatory

	// these are fixed and allocated at object construction
	pthreader_params * thread_params; // n_threads length array of pthreader_params objects to pass to threads

	// functions held here, too, for "master" thread evaluations
	pthreader_setup_fcn thread_alloc; 	// setup ("allocate") function handle
	pthreader_eval_fcn thread_eval; 	// evaluate function handle
	pthreader_free_fcn thread_free; 	// cleanup ("free") function handle

	void * eval_params; // any data to pass through; MUST BE ASSIGNED IN SETUP FUNCTION CALL PROVIDED

	// these are only created with launch(), and destroyed with close()
	int * workflag; 			// n_threads-1 length array to alert threads that work is available
	int * statflag; 			// n_threads length array for evaluation status
	pthread_mutex_t * worklock; // n_threads-1 length array of mutex locks for checking work
	pthread_cond_t * cv_work; 	// n_threads-1 length array of condition variables for work to do
	pthread_cond_t * cv_free; 	// n_threads-1 length array of condition variables for work done
	pthread_t * thread; 		// n_threads-1 length array of actual thread objects

	pthread_mutex_t prntlock; 	// for verbose

public:
	
	pthreader( int n_threads );

	void be_verbose();
	void be_quiet();

	void set_setup( pthreader_setup_fcn f );
	void set_evaluate( pthreader_eval_fcn f );
	void set_cleanup( pthreader_free_fcn f );

	void launch();					// launch without data
	void launch( void * data ); 	// launch with data
	
	void evaluate( void * in , void * out );

	void close();					// shutdown threads

	~pthreader();

};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * 
 * 
 * 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#endif