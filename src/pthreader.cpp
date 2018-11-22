
#include "pthreader.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * 
 * EVALUATE
 * 
 * do a multi-threaded evaluation
 * 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// for default initialization of objects
void * pthreader_setup_noop( int n , int N , void * data ) { return NULL; }
int pthreader_eval_noop( int n , void * params , void * in , void * out ) { return 0; }
void pthreader_close_noop( int n , void ** data ) {}

// ** IMPORTANT ** (I think)
// 
// don't mess with class-method objects, their naming can be weird if I recall correctly
// 
void * threaded_worker( void * arg ) 
{
	int i;
	pthreader_params * params = ( pthreader_params * )arg; // thread id included

	if( params->prnt && ( params->prntlock != NULL ) ) {
		pthread_mutex_lock( params->prntlock );
		printf( "launching thread %i / %i\n" , params->thrd + 1 , params->nthd ); fflush( stdout );
		pthread_mutex_unlock( params->prntlock );
	}

	// first things first... start by doing setup, and assigning the data pointer
	// in the parameter object passed in
	params->eval_params = ( params->thread_alloc )( params->thrd , params->nthd , params->init_data ); 

	// signal we are done with setup using cv_free condition variable
	// passed parameter structure says which lock this thread should use
	pthread_mutex_lock( params->worklock );
	(params->workflag)[0] = 0;
	pthread_cond_signal( params->cv_free );
	pthread_mutex_unlock( params->worklock );

	// work loop, waiting for signals that work is ready to do or that we're done
	while( 1 ) {

		// ensure mutual exclusivity of shared memory to check workflag
		pthread_mutex_lock( params->worklock );

		// if workflag isn't set, wait until it is
		if( (params->workflag)[0] == 0 ) { 
			pthread_cond_wait( params->cv_work , params->worklock );
			// (implicitly unlocks and, when done blocking, re-locks)
		}

		// if the parameters have their exit flag set, we need to exit
		// I think it is ok to not coordinate if we're killing threads... 
		if( params->exit == 1 ) {
			// clean up any allocated data in setup
			if( params->thread_free != NULL ) {
				(params->thread_free)( params->thrd , &(params->eval_params) ); 
			}
			pthread_exit( NULL ); // kill the thread
			return NULL;
		}

		// otherwise, do work
		(params->statflag)[0] = (params->thread_eval)( params->thrd , params->eval_params , params->eval_in , params->eval_out );

		// signal that we are done
		(params->workflag)[0] = 0; // clear workflag because work is done
		pthread_cond_signal( params->cv_free ); // signal that work is done

		// we're done with mutual exclusivity
		pthread_mutex_unlock( params->worklock );

	}

	return NULL;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * 
 * CONSTRUCTOR/DESTRUCTOR
 * 
 * build data up
 * 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

pthreader::pthreader( int n ) : n_threads(n)
{
	int t;

	if( n_threads <= 0 ) {
		// throw some kind of error or "bad initialization" return
	}

	// we use this a bunch, so might as well just define it
	n_threads_minus_one = n_threads - 1;

	thread_alloc = pthreader_setup_noop;
	thread_eval = pthreader_eval_noop;
	thread_free = pthreader_close_noop;
	eval_params = NULL;

	// allocate space for the thread parameter objects
	thread_params = ( pthreader_params * )malloc( n_threads_minus_one * sizeof( pthreader_params ) );

	// initialize those objects
	for( t = 0 ; t < n_threads_minus_one ; t++ ) {

		thread_params[t].exit = 0; // don't exit
		thread_params[t].thrd = t+1; // the thread number (increment because this is running in thread "0")
		thread_params[t].nthd = n_threads; // total number of threads
		thread_params[t].prnt = verbose; // be verbose? (DEFAULT IS NO)

		thread_params[t].init_data 		= NULL; // default

		thread_params[t].thread_alloc 	= pthreader_setup_noop;
		thread_params[t].thread_eval  	= pthreader_eval_noop;
		thread_params[t].thread_free  	= pthreader_close_noop;

		thread_params[t].eval_params  	= NULL; // default
		thread_params[t].eval_in 	  	= NULL; // default
		thread_params[t].eval_out 	  	= NULL; // default

		thread_params[t].workflag 		= NULL;
		thread_params[t].statflag 		= NULL;
		thread_params[t].worklock 		= NULL;
		thread_params[t].cv_work  		= NULL;
		thread_params[t].cv_free  		= NULL;

		thread_params[t].prntlock 		= NULL;

	}

	threads_open = 0;

}

pthreader::~pthreader( )
{
	if( threads_open ) { close(); }
	if( verbose ) { pthread_mutex_destroy( &prntlock ); }
	free( thread_params );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * 
 * PRINTING
 * 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void pthreader::be_verbose()
{
	int t;
	if( verbose ) { return; } // ignore if we are verbose already
	verbose = 1; 
	pthread_mutex_init( &prntlock , NULL );
	for( t = 0 ; t < n_threads_minus_one ; t++ ) { 
		thread_params[t].prnt = verbose; 
		thread_params[t].prntlock = &prntlock; 
	}
}

void pthreader::be_quiet()
{
	int t;
	if( ! verbose ) { return; } // ignore if we are quiet already
	verbose = 0; 
	pthread_mutex_destroy( &prntlock );
	for( t = 0 ; t < n_threads_minus_one ; t++ ) { 
		thread_params[t].prnt = verbose; 
		thread_params[t].prntlock = NULL; 
	}
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * 
 * LAUNCH
 * 
 * setup and launch the actual threads
 * 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void pthreader::set_setup( pthreader_setup_fcn f )
{
	int t;
	thread_alloc = f; // store in this object's data
	for( t = 0 ; t < n_threads_minus_one ; t++ ) {
		thread_params[t].thread_alloc = f; // store in thread data
	}
}

void pthreader::set_evaluate( pthreader_eval_fcn f )
{
	int t;
	thread_eval = f; // store in this object's data
	for( t = 0 ; t < n_threads_minus_one ; t++ ) {
		thread_params[t].thread_eval = f; // store in thread data
	}
}

void pthreader::set_cleanup( pthreader_free_fcn f )
{
	int t;
	thread_free = f; // store in this object's data
	for( t = 0 ; t < n_threads_minus_one ; t++ ) {
		thread_params[t].thread_free = f; // store in thread data
	}
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * 
 * STATUS
 * 
 * get status flags
 * 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int pthreader::get_eval_status( int n )
{
	if( threads_open ) { return statflag[n]; }
	else { return 0; }
}

int pthreader::get_all_status_zero() 
{ 
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ALL_ZERO
	return all_status_zero; 
#else
	int flag = 1;
	for( int t = 0 ; t < n_threads ; t++ ) {
		if( statflag[t] != 0 ) { return 0; }
	}
	return flag;
#endif
}

int pthreader::get_all_status_positive() 
{ 
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ALL_POS
	return all_status_pos; 
#else
	int flag = 1;
	for( int t = 0 ; t < n_threads ; t++ ) {
		if( statflag[t] <= 0 ) { return 0; }
	}
	return flag;
#endif
}

int pthreader::get_all_status_negative() 
{ 
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ALL_NEG
	return all_status_neg; 
#else
	int flag = 1;
	for( int t = 0 ; t < n_threads ; t++ ) {
		if( statflag[t] >= 0 ) { return 0; }
	}
	return flag;
#endif
}

int pthreader::get_any_status_zero() 
{ 
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ANY_ZERO
	return any_status_pos; 
#else
	int flag = 0;
	for( int t = 0 ; t < n_threads ; t++ ) {
		if( statflag[t] == 0 ) { return 1; }
	}
	return flag;
#endif
}

int pthreader::get_any_status_positive() 
{ 
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ANY_POS
	return any_status_pos; 
#else
	int flag = 0;
	for( int t = 0 ; t < n_threads ; t++ ) {
		if( statflag[t] > 0 ) { return 1; }
	}
	return flag;
#endif
}

int pthreader::get_any_status_negative() 
{ 
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ANY_NEG
	return any_status_neg; 
#else
	int flag = 0;
	for( int t = 0 ; t < n_threads ; t++ ) {
		if( statflag[t] < 0 ) { return 1; }
	}
	return flag;
#endif
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * 
 * LAUNCH
 * 
 * setup and launch the actual threads
 * 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void pthreader::launch(  ) { launch(NULL); }

void pthreader::launch( void * data )
{
	int t;

	if( threads_open ) {
		if( verbose ) {
			printf( "Threads are already running. You have to close() before calling launch().\n" );
		}
		return;
	}

	if( verbose ) { 
		pthread_mutex_lock( &prntlock );
		printf( "launching %i threads...\n" , n_threads );
		pthread_mutex_unlock( &prntlock );
	}

	// actually allocate the workflag list
	workflag = ( int * )malloc( n_threads_minus_one * sizeof(int) );

	// allocate status flags (for all threads, including this central one)
	statflag = ( int * )malloc( n_threads * sizeof(int) );

	// allocate the mutexes, condition variables, and thread data structures
	worklock = ( pthread_mutex_t * )malloc( n_threads_minus_one * sizeof( pthread_mutex_t ) );
	cv_work  = ( pthread_cond_t * )malloc( n_threads_minus_one * sizeof( pthread_cond_t ) );
	cv_free  = ( pthread_cond_t * )malloc( n_threads_minus_one * sizeof( pthread_cond_t ) );
	thread 	 = ( pthread_t * )malloc( n_threads_minus_one * sizeof( pthread_t ) );

	// for each thread we spawn, create a thread and call the generic "worker" routine
	// this will, on launch, execute any setup routines specified
	for( t = 0 ; t < n_threads_minus_one ; t++ ) {

		// initialize the pthread construct parts
		workflag[t] = 1; // start ** with ** work when we run setup
		pthread_mutex_init( worklock + t , NULL );
		pthread_cond_init( cv_work + t , NULL );
		pthread_cond_init( cv_free + t , NULL );

		// set the right pointers in the thread_params objects
		thread_params[t].workflag  = workflag + t;
		thread_params[t].statflag  = statflag + t + 1; // increment by one here, as we store all of them

		thread_params[t].worklock  = worklock + t;
		thread_params[t].cv_work   = cv_work  + t;
		thread_params[t].cv_free   = cv_free  + t;
		thread_params[t].init_data = data;

		// actually create the threads... this should define eval_params in each thread
		pthread_create( thread + t , NULL , threaded_worker , (void*)( thread_params + t ) );

	}

	// we do this ourselves, here, too but don't need to set the pointers

	// print if we should... how if we aren't storing params? 
	if( verbose ) { 
		pthread_mutex_lock( &prntlock );
		printf( "launching thread %i / %i\n" , 1 , n_threads ); fflush( stdout );
		pthread_mutex_unlock( &prntlock );
	}

	// do setup, assigning the data pointer in the parameter object passed in...
	eval_params = thread_alloc( 0 , n_threads , data );

	// print if we want
	if( verbose ) {
		pthread_mutex_lock( &prntlock );
		printf( "thread %i is done setting up.\n" , 1 );
		pthread_mutex_unlock( &prntlock );
	}

	// wait until thread setup _completes_
	for( t = 0 ; t < n_threads_minus_one ; t++ ) {
		pthread_mutex_lock( worklock + t );
		if( workflag[t] == 1 ) { // if thread t is still working...
			pthread_cond_wait( cv_free + t , worklock + t ); // wait for it to signal done
		}
		if( verbose ) {
			pthread_mutex_lock( &prntlock );
			printf( "thread %i is done setting up.\n" , t+2 );
			pthread_mutex_unlock( &prntlock );
		}
		pthread_mutex_unlock( worklock + t );
	}

	// set the running flag
	threads_open = 1;

}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * 
 * EVALUATE
 * 
 * do a multi-threaded evaluation
 * 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void pthreader::evaluate( void * in , void * out )
{
	int t;

	if( ! threads_open ) {
		if( verbose ) {
			printf( "You have not launched any threads to evaluate over.\n" );
		}
		return;
	}

#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ALL_ZERO
	all_status_zero = 1;
#endif
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ALL_POS
	all_status_pos  = 1;
#endif
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ALL_NEG
	all_status_neg  = 1;
#endif
	
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ANY_ZERO
	any_status_zero = 0;
#endif
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ANY_POS
	any_status_pos  = 0;
#endif
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ANY_NEG
	any_status_neg  = 0;
#endif

	// loop through worker threads storing data object and signaling that work is available
	for( t = 0 ; t < n_threads_minus_one ; t++ ) { 
		pthread_mutex_lock( worklock + t );
		if( workflag[t] == 1 ) {
			// wait for this thread to get free... but it should be free already...
			// note this call implicitly unlocks and, when ready, relocks the mutex
			pthread_cond_wait( cv_free + t , worklock + t ); 
		}
		thread_params[t].eval_in  = in;  // store passed data object now that it is safe
		thread_params[t].eval_out = out; // store passed data object now that it is safe
		workflag[t] = 1; // set shared memory flag to one to declare work
		pthread_cond_signal( cv_work + t ); // signal work is available
		pthread_mutex_unlock( worklock + t ); // unlock the mutex for this thread
	}

	// do work here, in this thread, too... using params constructed with setup fcn
	statflag[0] = thread_eval( 0 , eval_params , in , out );

#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ALL_ZERO
	all_status_zero = ( statflag[0] == 0 ? all_status_zero : 0 );
#endif
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ALL_POS
	all_status_pos  = ( statflag[0]  > 0 ? all_status_pos : 0 );
#endif
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ALL_NEG
	all_status_neg  = ( statflag[0]  < 0 ? all_status_neg : 0 );
#endif

#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ANY_ZERO
	any_status_zero = ( statflag[0] == 0 ? 1 : any_status_zero );
#endif
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ANY_POS
	any_status_pos  = ( statflag[0]  > 0 ? 1 : any_status_pos );
#endif
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ANY_NEG
	any_status_neg  = ( statflag[0]  < 0 ? 1 : any_status_neg );
#endif

	if( verbose ) {
		pthread_mutex_lock( &prntlock );
		printf( "thread %i is done evaluating.\n" , 1 );
		pthread_mutex_unlock( &prntlock );
	}

	// wait until evaluations complete
	for( t = 0 ; t < n_threads_minus_one ; t++ ) {
		pthread_mutex_lock( worklock + t );
		if( workflag[t] == 1 ) { // if thread t is still working...
			pthread_cond_wait( cv_free + t , worklock + t ); // wait for it to signal done
		}

#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ALL_ZERO
		all_status_zero = ( statflag[t+1] == 0 ? all_status_zero : 0 );
#endif
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ALL_POS
		all_status_pos  = ( statflag[t+1]  > 0 ? all_status_pos : 0 );
#endif
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ALL_NEG
		all_status_neg  = ( statflag[t+1]  < 0 ? all_status_neg : 0 );
#endif

#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ANY_ZERO
		any_status_zero = ( statflag[t+1] == 0 ? 1 : any_status_zero );
#endif
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ANY_POS
		any_status_pos  = ( statflag[t+1] >  0 ? 1 : any_status_pos );
#endif
#ifdef _PTHREADER_COMPILE_ACCUM_EVAL_STATUS_ANY_NEG
		any_status_neg  = ( statflag[t+1] <  0 ? 1 : any_status_neg );
#endif

		if( verbose ) {
			pthread_mutex_lock( &prntlock );
			printf( "thread %i knows thread %i is done evaluating.\n" , 1 , t+2 );
			pthread_mutex_unlock( &prntlock );
		}
		pthread_mutex_unlock( worklock + t );
	}
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * 
 * CLOSE
 * 
 * close down threads
 * 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void pthreader::close( ) 
{
	int t;

	// print if we want
	if( verbose ) {
		pthread_mutex_lock( &prntlock );
		printf( "shutting down worker %i threads...\n" , n_threads );
		pthread_mutex_unlock( &prntlock );
	}

	// signal each thread that it needs to stop working, clean up, and shut down
	for( t = 0 ; t < n_threads_minus_one ; t++ ) {
		pthread_mutex_lock( worklock + t );
		if( workflag[t] == 1 ) {
			pthread_cond_wait( cv_free + t , worklock + t );
		}
		thread_params[t].exit = 1; // set exit flag in the data structure accessed by worker thread t+1
		workflag[t] = 1;
		pthread_cond_signal( cv_work + t );
		pthread_mutex_unlock( worklock + t );
	}

	// do cleanup here too... 
	if( thread_free != NULL ) { thread_free( 0 , &eval_params ); }

	// join threads and cleanup infrastructure
	for( t = 0 ; t < n_threads_minus_one ; t++ ) {

		// destroy the threads by joining with this one
		pthread_join( thread[t] , NULL );

		// cleanup after pthreads
		pthread_mutex_destroy( worklock + t );
		pthread_cond_destroy( cv_work + t );
		pthread_cond_destroy( cv_free + t );

		// clear pointers in the thread_params objects
		thread_params[t].workflag = NULL;
		thread_params[t].worklock = NULL;
		thread_params[t].cv_work  = NULL;
		thread_params[t].cv_free  = NULL;
		thread_params[t].init_data = NULL;
		thread_params[t].eval_params = NULL;
		thread_params[t].eval_in = NULL;
		thread_params[t].eval_out = NULL;

	}

	// free the workflag list allocated by launch()
	free( workflag );

	// free the status flags
	free( statflag );

	free( worklock );
	free( cv_work  );
	free( cv_free  );
	free( thread   );

	// set the running flag
	threads_open = 0;

}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * 
 * 
 * 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
