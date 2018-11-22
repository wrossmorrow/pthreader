
#include <stdio.h>
#include <stdlib.h>

#include <gsl/gsl_multimin.h>

#include "pthreader.h"

double urand() { return ((double)rand())/((double)RAND_MAX); }

typedef struct pt_ols_params {
	int Nobsv;
	int Nthrd;
	int Nfeat;
	int Nvars;
	double * c;	// length Nfeat if not const, o/w Nfeat + 1
} pt_ols_params;

typedef struct pt_ols_data {
	int Nobsv;
	int Nfeat;
	int Nvars;
	double * D;
	double * y;
	double * r;
} pt_ols_data;

void * pt_ols_setup( int n , int N , void * args )
{
	int i , j , B , R;
	pt_ols_params * params = ( pt_ols_params * )args;
	pt_ols_data * data = ( pt_ols_data * )malloc( sizeof( pt_ols_data ) );

	// Remainder and block size, from thread number and number of threads
	R = ( params->Nobsv ) % N;
	B = ( params->Nobsv - R ) / N;

	// store sizes
	data->Nobsv = B + ( n < R ? 1 : 0 );
	data->Nfeat = params->Nfeat;
	data->Nvars = params->Nvars;

	// allocate space
	data->r = ( double * )malloc( data->Nobsv * sizeof( double ) );
	data->y = ( double * )malloc( data->Nobsv * sizeof( double ) );
	data->D = ( double * )malloc( ( data->Nvars * data->Nobsv ) * sizeof( double ) );

	// ok, actually fill in the data matrix D and observations y
	// we also "touch" the memory allocated for r, to make sure it is 
	// paged
	for( i = 0 ; i < data->Nobsv ; i++ ) {
		(data->y)[i] = 0.0;
		for( j = 0 ; j < data->Nvars ; j++ ) {
			(data->D)[(data->Nvars)*i+j] = ( j == data->Nfeat ? 1.0 : 2.0 * urand() - 1.0 );
			(data->y)[i] += (data->D)[(data->Nvars)*i+j] * (params->c)[j];
		}
		(data->r)[i] = 0.0;
	}

	return (void*)data;

}

void pt_ols_cleanup( int n , void ** arg )
{
	pt_ols_data * data = ( pt_ols_data * )(arg[0]);
	free( data->r );
	free( data->y );
	free( data->D );
	free( arg[0]  );
}

typedef struct pt_ols_eval_input {
	int type;
	double * x;
} pt_ols_eval_input;

typedef struct pt_ols_eval_results {
	int type;
	double * s;
	double * g;
} pt_ols_eval_results;

static double * x = NULL;
static double * sos = NULL;
static pt_ols_params params;
static pt_ols_input input;
static pt_ols_result result;

int pt_ols_evaluation( int n , void * data , void * in , void * out )
{
	int i , j;
	pt_ols_data * p = ( pt_ols_data * )data;
	pt_ols_eval_input * eval = ( pt_ols_eval_input  * )in;
	pt_ols_eval_result * res = ( pt_ols_eval_result * )out;

	switch( eval->type == 0 ) {

		case 0 : // f only
			// r <- D x - y  and  s[n] <- r' r
			(res->s)[n] = 0.0;
			for( i = 0 ; i < p->Nobsv ; i++ ) {
				(p->r)[i] = - (p->y)[i];
				for( j = 0 ; j < p->Nvars ; j++ ) {
					(p->r)[i] += (p->D)[ (p->Nvars)*i + j ] * x[j]; // D stored column major
				}
				(res->s)[n] += (p->r)[i] * (p->r)[i];
			}
			(res->s)[n] /= 2.0; // typical normalization for sums of squares
			break;

		case 1 : // df only
			// r <- D x - y  and  g <- D' r
			for( i = 0 ; i < p->Nobsv ; i++ ) {
				(p->r)[i] = - (p->y)[i];
				for( j = 0 ; j < p->Nvars ; j++ ) {
					(p->r)[i] += (p->D)[ (p->Nvars)*i + j ] * x[j]; // D stored column major
				}
			}
			for( j = 0 ; j < p->Nvars ; j++ ) {
				(res->g)[(p->Nvars)*n+i] = 0.0;
				for( i = 0 ; i < p->Nobsv ; i++ ) {
					(res->g)[(p->Nvars)*n+i] += (p->D)[ (p->Nvars)*i + j ] * (p->r)[i]; // D stored column major
				}
			}
			break;

		case 2 : // f and df
			// r <- D x - y  and  s[n] <- r' r
			(res->s)[n] = 0.0;
			for( i = 0 ; i < p->Nobsv ; i++ ) {
				(p->r)[i] = - (p->y)[i];
				for( j = 0 ; j < p->Nvars ; j++ ) {
					(p->r)[i] += (p->D)[ (p->Nvars)*i + j ] * x[j]; // D stored column major
				}
				(res->s)[n] += (p->r)[i] * (p->r)[i];
			}
			(res->s)[n] /= 2.0; // typical normalization for sums of squares
			for( j = 0 ; j < p->Nvars ; j++ ) {
				(res->g)[(p->Nvars)*n+i] = 0.0;
				for( i = 0 ; i < p->Nobsv ; i++ ) {
					(res->g)[(p->Nvars)*n+i] += (p->D)[ (p->Nvars)*i + j ] * (p->r)[i]; // D stored column major
				}
			}
			break;

	}

	return 0;
}

void prepare_inputs( gsl_vector * vars , pt_ols_input * in )
{
	if( vars->stride == 1 ) { in->x = vars->data; }
	else { // buffer the gsl_vector into a contiguous array
		if( x == NULL ) {
			x = ( double * )malloc( vars->size * sizeof( double ) );
		}
		for( int i = 0 ; i < vars->size ; i++ ) {
			x[i] = gsl_vector_get( vars , i );
		}
		in->x = x;
	}
}

// GSL objective routines for function only, gradient only, and both

double threaded_objective_f( gsl_vector * vars , void * data )
{
	int i;
	pthreader * PT = ( pthreader * )data;

	prepare_inputs( vars , &input );
	input.type = 0;

	PT->evaluate( (void*)(&input) , (void*)(&results) );

	double f = 0.0;
	for( i = 0 ; i < params.Nthrd ; i++ ) { f += (result->s)[i]; }
	f /= ((double)params.Nobsv);
}

void threaded_objective_df( gsl_vector * vars , gsl_vector * df , void * data )
{
	int i , j;
	pthreader * PT = ( pthreader * )data;

	prepare_inputs( vars , &input );
	input.type = 1;

	PT->evaluate( (void*)(&input) , (void*)(&results) );

	for( i = 0 ; i < params.Nthrd ; i++ ) { 
		gsl_vector_set( df , j , 0.0 );
		for( j = 0 ; j < params.Nvars ; j++ ) { 
			gsl_vector_set( df , j , gsl_vector_get(df,j) + (result.g)[(params.Nvars)*i+j] );
		}
		gsl_vector_set( df , j , gsl_vector_get(df,j) / ((double)params.Nobsv) );
	}
	for( j = 0 ; j < params.Nvars ; j++ ) { 
		gsl_vector_set( df , j , gsl_vector_get(df,j) / ((double)params.Nobsv) );
	}
}

double threaded_objective_fdf( gsl_vector * vars , double * f , gsl_vector * df , void * data )
{
	int i , j;
	pthreader * PT = ( pthreader * )data;

	prepare_inputs( vars , &input );
	input.type = 2;

	PT->evaluate( (void*)(&input) , (void*)(&result) );

	f[0] = 0.0;
	for( i = 0 ; i < params.Nthrd ; i++ ) { 
		f[0] += (result.s)[i]; 
		gsl_vector_set( df , j , 0.0 );
		for( j = 0 ; j < params.Nvars ; j++ ) { 
			gsl_vector_set( df , j , gsl_vector_get(df,j) + (result.g)[(params.Nvars)*i+j] );
		}
	}
	for( j = 0 ; j < params.Nvars ; j++ ) { 
		gsl_vector_set( df , j , gsl_vector_get(df,j) / ((double)params.Nobsv) );
	}
	f[0] /= ((double)params.Nobsv);
	
}

// optimization setup
int minimize_wo_grad( gsl_vector * gsl_x0 , double * xs , void * params , double opt_tol , int max_iter )
{

	int i , iter = 0 , status;
	double size;

	// minimizer object
	const gsl_multimin_fminimizer_type * T = gsl_multimin_fminimizer_nmsimplex2;
	gsl_multimin_fminimizer * s = gsl_multimin_fminimizer_alloc( T , params.Nvars );

	// evaluation function
	gsl_multimin_function obj;
	obj.n 		= params.Nvars; // features and constant
	obj.f 		= &threaded_objective_f; // defined elsewhere
	obj.params 	= params; // we'll pass the data object, allocated here, to objective evaluations

	// step size
	gsl_vector * ss = gsl_vector_alloc( params.Nvars );
	gsl_vector_set_all( ss , 1.0 );
	
	// "register" these with the minimizer, which _WILL_ call objective
	gsl_multimin_fminimizer_set( s , &obj , gsl_x0 , ss );

	// iterations
	status = GSL_CONTINUE;
	do {

		iter++;

		// iterate will call the distributed objective
		status = gsl_multimin_fminimizer_iterate( s );

		if( status ) { break; } // iteration failure? 

		size = gsl_multimin_fminimizer_size( s );
		status = gsl_multimin_test_size( size , opt_tol );

	} while( status == GSL_CONTINUE && iter < max_iter );

	for( i = 0 ; i < params.Nvars ; i++ ) {
		xs[i] = gsl_vector_get( s->x , i );
	}

	// clean up after optimizer
	gsl_vector_free( gsl_x0 );
	gsl_vector_free( ss );
	gsl_multimin_fminimizer_free( s );

	return status;

}


// optimization setup
int minimize_w_grad( gsl_vector * gsl_x0 , double * xs , void * params , double opt_tol , int max_iter )
{

	int i , iter = 0 , status;
	double step_size = 1.0;

	// minimizer object
	const gsl_multimin_fdfminimizer_type * T = gsl_multimin_fdfminimizer_vector_bfgs;
	gsl_multimin_fdfminimizer * s = gsl_multimin_fdfminimizer_alloc( T , params.Nvars );

	// evaluation function
	gsl_multimin_function_fdf obj;
	obj.n = params.Nvars; // features and constant
	obj.f = &threaded_objective_f; // defined elsewhere
	obj.df = &threaded_objective_df;
	obj.fdf = &threaded_objective_fdf;
	obj.params = params; // we'll pass the data object, allocated here, to objective evaluations

	// initial point (random guess)
	gsl_vector * x = gsl_vector_alloc( params->Nvars );
	for( i = 0 ; i < params->Nvars ; i++ ) { gsl_vector_set( x , i , x0[i] ); }

	// "register" these with the minimizer
	gsl_multimin_fdfminimizer_set( s , &obj , x , step_size , opt_tol );

	// iterations
	status = GSL_CONTINUE;
	do {

		// iterate will call the distributed objective
		status = gsl_multimin_fdfminimizer_iterate( s );
		iter++;

		if( status ) { break; } // iteration failure? 

		status = gsl_multimin_test_gradient( s->gradient , opt_tol );

	} while( status == GSL_CONTINUE && iter < max_iter );
	
	for( i = 0 ; i < params.Nvars ; i++ ) {
		xs[i] = gsl_vector_get( s->x , i );
	}

	// clean up after optimizer
	gsl_vector_free( gsl_x0 );
	gsl_vector_free( ss );
	gsl_multimin_fminimizer_free( s );

	return status;

}


int main( int argc , char * argv[] ) 
{
	// read T, N, K, and const from CL args

	if( argc < 5 ) {
		printf( "\"%s\" expects four arguments: Number of Threads, Number of Observations, Number of Features, and Constant (yes/no)\n" , argv[0] );
		return 1;
	}

	params.Nthrd = (int)strtol( argv[1] , NULL , 10 );
	params.Nobsv = (int)strtol( argv[2] , NULL , 10 );
	params.Nfeat = (int)strtol( argv[3] , NULL , 10 );
	params.Nvars = params.Nfeat + ( (int)strtol( argv[4] , NULL , 10 ) ? 1 : 0 );

	if( params.Nthrd <= 1 ) { 
		printf( "\"%s\" expects at least two threads\n" , argv[0] );
		return 1;
	}

	if( params.Nobsv <= params.Nthrd ) { 
		printf( "\"%s\" expects at least as many observations as threads\n" , argv[0] );
		return 1;
	}

	if( params.Nfeat <= 0 ) { 
		printf( "\"%s\" expects a positive number of features\n" , argv[0] );
		return 1;
	}

	// allocate some memory for "real" coefficients
	params.c = ( double * )malloc( params.Nvars * sizeof( double ) );
	for( int i = 0 ; i < params.Nvars ; i++ ) { (params.c)[i] = 2.0 * urand() - 1.0; }

	// create a new pthreader object with the number of threads
	pthreader * PT = new pthreader( params.Nthrd );

	// print out what is happening during setup
	PT->be_verbose();

	// define what to do
	PT->set_setup( pt_ols_setup );
	PT->set_evaluate( pt_ols_evaluation );
	PT->set_cleanup( pt_ols_cleanup );

	// launch the threads, with initial data
	PT->launch( (void*)(&params) );

	// storage for sums of squares
	input.s = ( double * )malloc( params.Nthrd * sizeof( double ) );
	input.g = ( double * )malloc( ( params.Nthrd * params.Nvars ) * sizeof( double ) );

	double * x0 = NULL;

	// be quiet for the optimization
	PT->be_quiet();

	// optimization
	minimize_wo_grad( gsl_x0 , &x , (void*)(PT) , 1.0e-4 , 1000 );

	minimize_w_grad( gsl_x0 , &x , (void*)(PT) , 1.0e-4 , 1000 );

	// non-verbose print
	printf( "%0.6f: real coeffs: %0.3f" , now() , params.c[0] );
	for( i = 1 ; i < params.Nvars ; i++ ) { printf( " , %0.3f" , params.c[i] ); }
	printf( "\n" );
	
	// non-verbose print
	printf( "%0.6f: estimated coeffs: %0.3f" , now() , x[0] );
	for( i = 1 ; i < params.Nvars ; i++ ) { printf( " , %0.3f" , x[i] ); }
	printf( "\n" );

	// print out what is happening again
	PT->be_verbose();

	// close the threads
	PT->close( );

	// release memory
	free( params.c );

	// free sums-of-squares memory
	free( result.s );
	free( result.g );

	// leave
	return 0;

}

