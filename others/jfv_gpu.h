#include <cuda_runtime.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <vector>

void test();

void add(int a, int b, int *c);

void vonoi( int SizeX , int SizeY , const float2 * SiteArray , const int * Ping , int * Pong , int k , int * Mutex);
