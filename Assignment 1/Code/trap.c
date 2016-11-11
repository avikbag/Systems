/*  Purpose: Calculate definite integral using the trapezoidal rule.
 *
 * Input:   a, b, n
 * Output:  Estimate of integral from a to b of f(x)
 *          using n trapezoids.
 *
 * Author: Naga Kandasamy, Michael Lui
 * Date: 6/22/2016
 *
 * Compile: gcc -o trap trap.c -lpthread -lm
 * Usage:   ./trap
 *
 *
 * Note:    The function f(x) is hardwired.
 *
 */

#include "pthread.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>

#define LEFT_ENDPOINT 5
#define RIGHT_ENDPOINT 1000
#define NUM_TRAPEZOIDS 100000000
#define NUM_THREADs 4 /* Number of threads to run. */


/*------------------------------------------------------------------
 * Function:    func
 * Purpose:     Compute value of function to be integrated
 * Input args:  x
 * Output: (x+1)/sqrt(x*x + x + 1)
 */
__attribute__((const)) float func(float x)
{
    return (x + 1)/sqrt(x*x + x + 1);
}


struct ThreadArgument {
    int start;
    int end;
    int n;
    float(*f)(float);
    double result;
};

double compute_gold(float, float, int, float (*)(float));
double compute_using_pthreads(float, float, int, float (*)(float));

int main(int argc, char *argv[])
{
    int n = NUM_TRAPEZOIDS;
    float a = LEFT_ENDPOINT;
    float b = RIGHT_ENDPOINT;

    double reference = compute_gold(a, b, n, func);
    printf("Reference solution computed on the CPU = %f \n", reference);

    double pthread_result = compute_using_pthreads(a, b, n, func); /* Write this function using pthreads. */
    printf("Solution computed using pthreads = %f \n", pthread_result);
}

/*------------------------------------------------------------------
 * Function:    Trap
 * Purpose:     Estimate integral from a to b of f using trap rule and
 *              n trapezoids
 * Input args:  a, b, n, f
 * Return val:  Estimate of the integral
 */
double compute_gold(float a, float b, int n, float(*f)(float))
{
    float h = (b-a)/(float)n; /* 'Height' of each trapezoid. */

    double integral = (f(a) + f(b))/2.0;

    for (int k = 1; k <= n-1; k++)
        integral += f(a+k*h);

    integral = integral*h;

    return integral;
}

/*------------------------------------------------------------------
 * Function:    Trap Thread
 */
void *compute_thread_gold(void *argp)
{
    struct ThreadArgument *args = argp;
    double res = compute_gold(args->start, args->end, args->n, args->f);

    args->result = res;
}

double compute_using_pthreads(float a, float b, int n, float(*f)(float))
{
    int trapezoidPerThread = NUM_TRAPEZOIDS / NUM_THREADs;
    int pointsPerThread = (RIGHT_ENDPOINT - LEFT_ENDPOINT) / NUM_THREADs;

    struct ThreadArgument threadArgs[NUM_THREADs];

    pthread_t t_gold[NUM_THREADs];

    for (int i = 0; i < NUM_THREADs; i++) {
        int start = LEFT_ENDPOINT + pointsPerThread * i;
        int end = start + pointsPerThread;

        if (i == NUM_THREADs - 1) {
            end = RIGHT_ENDPOINT;
        }

        threadArgs[i].start = start;
        threadArgs[i].end = end;
        threadArgs[i].n = trapezoidPerThread;
        threadArgs[i].f = func;

        pthread_create(&t_gold[i], NULL, compute_thread_gold, &threadArgs[i]);
    }

    double result = 0;

    for (int i = 0; i < NUM_THREADs; i++) {
        pthread_join(t_gold[i], NULL);

        result += threadArgs[i].result;
    }

    return result;
}