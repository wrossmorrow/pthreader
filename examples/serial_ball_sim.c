
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

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

unsigned long simulate( const int T , const int N , unsigned int * U )
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

int main( int argc , char * argv[] )
{
    int N = atoi(argv[1]) , T = atoi(argv[2]);
    unsigned int * U;
    double P, Pp, F, I = 0.0, Q;

    srand(time(0)); 

    U = (unsigned int *)malloc( N*N * sizeof(unsigned int) );

    P = ((double)simulate(T, N, U))/((double)T); 
    Pp = 1.0 - P;
    while( fabs( Pp - P ) > 1.0e-6 ) {
        printf( "  current probability: %0.6f (%0.7f) (%i)\n" , P , fabs( Pp - P ) , (int)((I+1)*T) );
        Pp = P; I += 1.0;
        Q = ((double)simulate(T, N, U))/((double)T);
        P = ( (1.0-1.0/I) * P + 1.0/I * Q );
    }

    printf( "probability the same: %0.6f\n" , P );
    F = 1.0 - pow( 1.0-1.0/((double)N) , (double)N );
    printf( "   prithvi's formula: %0.6f\n" , F );
    F = 1.0 - ((double)(N-1))/((double)N) * pow( ((double)(N*N-N+1))/((double)(N*N)) , (double)N-1.0 );
    printf( "       ross' formula: %0.6f\n" , F );

    free(U);

}
