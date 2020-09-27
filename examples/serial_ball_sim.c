
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define RS_SCALE ( 1.0 / ( 1.0 + RAND_MAX ) )

double drand(void) 
{
    double d;
    do {
       d = ( ( (rand()*RS_SCALE) + rand() ) * RS_SCALE + rand() ) * RS_SCALE;
    } while( d >= 1 ); /* Round off */
    return d;
}

#define irand(x) ( (unsigned int) ((x) * drand ()) )

int randbiased( double x ) 
{
    for(;;) {
        double p = rand() * RS_SCALE;
        if( p >= x ) return 0;
        if( p+RS_SCALE <= x ) return 1;
        /* p < x < p+RS_SCALE */
        x = (x - p) * (1.0 + RAND_MAX);
    }
}

unsigned int randrange( unsigned int n ) 
{
    double xhi , resolution = n * RS_SCALE;
    double x = resolution * rand(); /* x in [0,n) */
    unsigned int lo = (unsigned int)floor(x);

    xhi = x + resolution;

    for (;;) {
        lo++;
        if( lo >= xhi || randbiased( (lo-x)/(xhi-x) ) )
            return lo-1;
        x = lo;
    }

}

double random_u( ) { return ((double)rand())/((double)RAND_MAX); }

unsigned int randuint( int b ) 
{
    return (unsigned int)( (double)b * random_u() );
}

unsigned int simulate( const int T , const int N , unsigned int * U )
{
    int t, r, c;
    unsigned int B1 , B2;
    unsigned int C = 0;

    for( t = 0 ; t < T ; t++ ) {
        for( r = 0 ; r < N ; r++ )
            for( c = 0 ; c < N ; c++ )
                U[N*r+c] = randrange(N);
        B1 = randrange(N);
        B2 = randrange(N);
        for( r = 0 ; r < N ; r++ ) {
            B1 = U[N*r+B1];
            B2 = U[N*r+B2];
        }
        if( B1 == B2 ) C++;
    }
    return C;
}

int main( int argc , char * argv[] )
{
    int N = atoi(argv[1]) , T = atoi(argv[2]), I = 0;
    unsigned int * U , C = 0;
    double P, Pp, F, Td = (double)T, Nd = (double)N;

    srand(time(0)); 

    U = (unsigned int *)malloc( N*N * sizeof(unsigned int) );

    C = simulate(T, N, U);
    P = ((double)C) / Td; 
    Pp = 1.0 - P; I = T;
    while( fabs( Pp - P ) > 1.0e-6 ) {
        printf( "  current probability: %0.6f (%0.7f) (%i)\n" , P , fabs( Pp - P ) , I );
        Pp = P; I += T; Td = ((double)I);
        C += simulate(T, N, U);
        P  = ((double)C) / Td;
    }

    printf( "probability the same: %0.6f\n" , P );
    F = 1.0 - pow( 1.0-1.0/Nd , Nd );
    printf( "   prithvi's formula: %0.6f\n" , F );
    F = 1.0 - (Nd-1.0)/Nd * pow( (Nd*Nd-N+1.0)/(Nd*Nd) , Nd-1.0 );
    printf( "       ross' formula: %0.6f\n" , F );

    free(U);

}
