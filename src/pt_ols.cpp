
#include <stdio.h>
#include <stdlib.h>

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

int pt_ols_evaluation( int n , void * data , void * in , void * out )
{
	int i , j;
	double * x = ( double * )in;
	pt_ols_data * p = ( pt_ols_data * )data;
	double * s = ( double * )out;

	// r <- D x - y  and  s[n] <- r' r
	s[n] = 0.0;
	for( i = 0 ; i < p->Nobsv ; i++ ) {
		(p->r)[i] = - (p->y)[i];
		for( j = 0 ; j < p->Nvars ; j++ ) {
			(p->r)[i] += (p->D)[ (p->Nvars)*i + j ] * x[j]; // D stored column major
		}
		s[n] += (p->r)[i] * (p->r)[i];
	}
	s[n] /= 2.0; // typical normalization for sums of squares

	return 0;
}

int main( int argc , char * argv[] ) 
{
	// read T, N, K, and const from CL args

	if( argc < 5 ) {
		printf( "\"%s\" expects four arguments: Number of Threads, Number of Observations, Number of Features, and Constant (yes/no)\n" , argv[0] );
		return 1;
	}

	pt_ols_params params;

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

	// allocate some memory for coefficients
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

	// here we can do any evaluations we want
	double * x = ( double * )malloc( params.Nvars * sizeof( double ) );
	double * s = ( double * )malloc( params.Nthrd * sizeof( double ) );
	double S = 0.0;

	// be quiet for the evaluations
	PT->be_quiet();

	// do several evaluations, to show calls can be repeated
	for( int iter = 0 ; iter < 10 ; iter++ ) {

		for( int i = 0 ; i < params.Nvars ; i++ ) { x[i] = 2.0 * urand() - 1.0; }

		PT->evaluate( (void*)x , (void*)s );
		
		S = 0.0;
		for( int t = 0 ; t < params.Nthrd ; t++ ) { S += s[t]; }
		S /= ((double)(params.Nobsv));
		printf( "evaluated, and obtained: %0.6f\n" , S );

	}

	free( x );
	free( s );

	// print out what is happening again
	PT->be_verbose();

	// close the threads
	PT->close( );

	// release memory
	free( params.c );

	// leave
	return 0;

}

