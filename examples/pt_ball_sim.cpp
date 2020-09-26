
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "pthreader.h"

// http://www.azillionmonkeys.com/qed/random.html

#define RS_SCALE (1.0 / (1.0 + RAND_MAX))

double drand (void) {
    double d;
    do {
       d = (((rand () * RS_SCALE) + rand ()) * RS_SCALE + rand ()) * RS_SCALE;
    } while (d >= 1); /* Round off */
    return d;
}

#define irand(x) ((unsigned int) ((x) * drand ()))

int randbiased (double x) {
    for (;;) {
        double p = rand () * RS_SCALE;
        if (p >= x) return 0;
        if (p+RS_SCALE <= x) return 1;
        /* p < x < p+RS_SCALE */
        x = (x - p) * (1.0 + RAND_MAX);
    }
}

size_t randrange(size_t n) {
    double xhi;
    double resolution = n * RS_SCALE;
    double x = resolution * rand (); /* x in [0,n) */
    size_t lo = (size_t) floor (x);

    xhi = x + resolution;

    for (;;) {
        lo++;
        if (lo >= xhi || randbiased ((lo - x) / (xhi - x))) return lo-1;
        x = lo;
    }
}

double random_u( ) { return ((double)rand())/((double)RAND_MAX); }

unsigned int randuint( int a , int b ) 
{ 
    if( b == a ) return b;
    if( b <  a ) return randuint(b,a);
    return (unsigned int)( (double)(b-a) * random_u() + a );
}

unsigned long int simulate( const int T , const int N , unsigned int * U )
{
    int t, r, c;
    unsigned int B1 , B2;
    unsigned long int C = 0l;

    for( t = 0 ; t < T ; t++ ) {
        for( r = 0 ; r < N ; r++ )
            for( c = 0 ; c < N ; c++ )
                U[N*r+c] = (unsigned int)randrange(N);
        B1 = (unsigned int)randrange(N);
        B2 = (unsigned int)randrange(N);
        for( r = 0 ; r < N ; r++ ) {
            B1 = U[N*r+B1];
            B2 = U[N*r+B2];
        }
        if( B1 == B2 )
            C += 1l;
    }
    return C;
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
    data->U  = ( unsigned int * )malloc( (params->N)*(params->N) * sizeof(unsigned int) );

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
    unsigned long int * C = ( unsigned long int * )out;

    // Remainder and block size, from thread number and number of threads
    // make sure we split evenly by spreading remainder over threads
    R = ( Tt ) % ( d->Nt );
    B = ( Tt - R ) / ( d->Nt );
    T = B + ( n < R ? 1 : 0 );

    // simulate, storing result in output array location for this thread
    C[n] = simulate( T , d->N , d->U );

    return 0;
}

unsigned long int sum( const int N , const unsigned long int * x )
{
    unsigned long int y = 0l; 
    for( int n = 0 ; n < N ; n++ ) y += x[n];
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
    unsigned long int * C = ( unsigned long int * )malloc( params.Nt * sizeof(unsigned long int) );
    int T = params.T0;
    int N = params.N;
    double Td = (double)T;
    double Nd = (double)N;

    PT->evaluate( (void*)(&T) , (void*)C );
    P = ((double)sum( params.Nt , C )) / ((double)T);
    Pp = 1.0 - P; // ensure condition fails

    double I = 0.0, Q;
    while( fabs( Pp - P ) > 1.0e-5 ) {
        printf( "+ current probability: %0.4f\n" , P );
        printf( "+       current count: %i\n" , (int)(I*T+T) );
        Pp = P; I += 1.0;
        PT->evaluate( (void*)(&T) , (void*)C );
        Q = ((double)sum( params.Nt , C )) / Td;
        P = ( (1.0-1.0/I) * P + 1.0/I * Q );
    }

    printf( "probability the same: %0.4f\n" , P );

    F = 1.0 - pow( 1.0 - 1.0/Nd , Nd );
    printf( "   prithvi's formula: %0.4f\n" , F );

    F = 1.0 - (Nd-1.0)/Nd * pow( (Nd*Nd-Nd+1.0)/(Nd*Nd) , Nd-1.0 );
    printf( "       ross' formula: %0.4f\n" , F );

    free( C );

    // close the threads
    PT->close( );

    return 0;

}
