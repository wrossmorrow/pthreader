
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "pthreader.h"

double random_u( ) { return ((double)rand())/((double)RAND_MAX); }

unsigned int randuint( int a , int b ) 
{ 
    if( b == a ) return b;
    if( b <  a ) return randuint(b,a);
    return (unsigned int)( (double)(b-a) * random_u() + a );
}

double simulate( const int T , const int N , unsigned int * U )
{
    int t, r, c;
    unsigned int B1 , B2;
    double P = 0.0;

    for( t = 0 ; t < T ; t++ ) {
        for( r = 0 ; r < N ; r++ )
            for( c = 0 ; c < N ; c++ )
                U[N*r+c] = randuint(0,N);
        B1 = randuint(0,N); B2 = randuint(0,N);
        for( r = 0 ; r < N ; r++ ) {
            B1 = U[N*r+B1]; B2 = U[N*r+B2];
        }
        if( B1 == B2 )
            P += 1.0;
    }
    // P /= ((double)T);
    return P;
}

typedef struct pt_sim_params {
    int Nt; // number of threads
    int N;  // grid size
    int T0; // initial number of trials
} pt_sim_params;

typedef struct pt_sim_data {
    int Nt; // number of threads
    int N; // grid size
    unsigned int * U; // grid data
} pt_sim_data;

void * pt_sim_setup( int n , int N , void * args )
{

    int i;
    pt_sim_params * params = ( pt_sim_params * )args;
    pt_sim_data * data = ( pt_sim_data * )malloc( sizeof( pt_sim_data ) );

    data->Nt = N;
    data->N  = params->N;
    data->U  = ( unsigned int * )malloc( (params->N)*(params->N)*sizeof(unsigned int) );

    // "touch" memory allocated to page it
    for( i = 0 ; i < (params->N)*(params->N) ; i++ )
        data->U[i] = 0;

    return (void*)data;

}

void pt_sim_cleanup( int n , void ** arg )
{
    pt_sim_data * data = ( pt_sim_data * )(arg[0]);
    free( data->U );
}

int pt_sim_evaluation( int n , void * data , void * in , void * out )
{

    int t, r, c, B, R, T, N;
    unsigned int B1, B2;

    pt_sim_data * d = ( pt_sim_data * )data;
    int Tt = *(( int * )in);
    double * P = ( double * )out;

    // Remainder and block size, from thread number and number of threads
    // make sure we split evenly by spreading remainder over threads
    R = ( Tt ) % ( d->Nt );
    B = ( Tt - R ) / ( d->Nt );
    T = B + ( n < R ? 1 : 0 );


    // simulate
    P[n] = simulate( T , d->N , d->U );
    // printf( "(%i) evaluate: Ttotal = %i, T = %i, P = %0.4f\n" , n , Tt, T, P[n]/((double)T) );

    return 0;
}

double aggregate( const int N , const int T , const double * x )
{
    double y = 0.0; 
    for( int n = 0 ; n < N ; n++ ) 
        y += x[n];
    y /= (double)T;
    return y;
}

int main( int argc , char * argv[] )
{

    pt_sim_params params;

    params.Nt = (int)strtol( argv[1] , NULL , 10 );
    params.N  = (int)strtol( argv[2] , NULL , 10 );
    params.T0 = (int)strtol( argv[3] , NULL , 10 );

    srand(time(0)); 

    // create a new pthreader object with the number of threads
    pthreader * PT = new pthreader( params.Nt );

    // print out what is happening during setup
    PT->be_verbose();

    // define what to do
    PT->set_setup( pt_sim_setup );
    PT->set_evaluate( pt_sim_evaluation );
    PT->set_cleanup( pt_sim_cleanup );

    // launch the threads, with initial data
    PT->launch( (void*)(&params) );

    PT->be_quiet();

    double P, Pp, F;
    double * Pt = ( double * )malloc( params.Nt * sizeof(double) );
    int T = params.T0;
    int N = params.N;

    PT->evaluate( (void*)(&T) , (void*)Pt );
    P = aggregate( N , T , Pt );
    Pp = 1.0 - P; // ensure condition fails

    while( fabs( Pp - P ) > 1.0e-5 ) {
        printf( "+ current probability: %0.4f\n" , P );
        printf( "+ doubling T: %i -> %i\n" , T , 2*T );
        T = 2*T; Pp = P;
        PT->evaluate( (void*)(&T) , (void*)Pt );
        P = aggregate( N , T , Pt );
    }

    printf( "probability the same: %0.4f\n" , P );

    F = 1.0 - pow( 1.0 - 1.0/((double)N) , (double)N );
    printf( "   prithvi's formula: %0.4f\n" , F );

    F = 1.0 - ((double)(N-1))/((double)N) * pow( ((double)(N*N-N+1))/((double)(N*N)) , (double)N-1.0 );
    printf( "       ross' formula: %0.4f\n" , F );

    free( Pt );

    // close the threads
    PT->close( );

    return 0;

}
