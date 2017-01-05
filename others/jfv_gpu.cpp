#include "jfv_gpu.h"
#include <math.h>
#include <fstream>
#include <iostream>
#include <algorithm>

extern "C" {
	void addOnDevice(int a, int b, int *c);
	void vonoiOnDevice( int SizeX , int SizeY , const float2 * SiteArray , const int * Ping , int * Pong , int k , int * Mutex);
}

void add(int a, int b, int *c) {
	addOnDevice(a, b, c);
}

void vonoi( int SizeX , int SizeY , const float2 * SiteArray , const int * Ping , int * Pong , int k , int * Mutex) {
	vonoiOnDevice( SizeX , SizeY , SiteArray , Ping , Pong , k , Mutex);
}

void test() {
	int NumSites = 100;
    int Size     = 512;

    //
    int NumCudaDevice = 0 ;
    cudaGetDeviceCount( & NumCudaDevice ) ;
	
	std::cout << NumCudaDevice << std::endl;
    //
    //
    std::vector< float2 > SiteVec ;
    std::vector< int >    SeedVec( Size * Size , - 1 ) ;
    std::vector< uchar3 > RandomColorVec ;
    for ( int i = 0 ; i < NumSites ; ++ i )
    {
        float X = static_cast< float >( rand() ) / RAND_MAX * Size ;
        float Y = static_cast< float >( rand() ) / RAND_MAX * Size ;
        int CellX = static_cast< int >( floorf( X ) ) ;
        int CellY = static_cast< int >( floorf( Y ) ) ;

        SiteVec.push_back( make_float2( CellX + 0.5f , CellY + 0.5f ) ) ;
        SeedVec[CellX + CellY * Size] = i ;

        RandomColorVec.push_back( make_uchar3( static_cast< unsigned char >( static_cast< float >( rand() ) / RAND_MAX * 255.0f ) ,
                                               static_cast< unsigned char >( static_cast< float >( rand() ) / RAND_MAX * 255.0f ) ,
                                               static_cast< unsigned char >( static_cast< float >( rand() ) / RAND_MAX * 255.0f ) ) ) ;
    }

    //
    size_t SiteSize = NumSites * sizeof( float2 ) ;

    float2 * SiteArray = NULL ;
    cudaMalloc( & SiteArray , SiteSize ) ;
    cudaMemcpy( SiteArray , & SiteVec[0] , SiteSize , cudaMemcpyHostToDevice ) ;

    //
    size_t BufferSize = Size * Size * sizeof( int ) ;

    int * Ping = NULL , * Pong = NULL ;
    cudaMalloc( & Ping , BufferSize ) , cudaMemcpy( Ping , & SeedVec[0] , BufferSize , cudaMemcpyHostToDevice ) ;
    cudaMalloc( & Pong , BufferSize ) , cudaMemcpy( Pong , Ping , BufferSize , cudaMemcpyDeviceToDevice ) ;

    //
    int * Mutex = NULL ;
    cudaMalloc( & Mutex , sizeof( int ) ) , cudaMemset( Mutex , - 1 , sizeof( int ) ) ;

    //
    //
    cudaDeviceProp CudaDeviceProperty ;
    cudaGetDeviceProperties( & CudaDeviceProperty , 0 ) ;

    for ( int k = Size / 2 ; k > 0 ; k = k >> 1 )
    {
        vonoi( Size , Size , SiteArray , Ping , Pong , k , Mutex) ;
        cudaDeviceSynchronize() ;

        cudaMemcpy( Ping , Pong , BufferSize , cudaMemcpyDeviceToDevice ) ;
        std::swap( Ping , Pong ) ;
    }
    cudaMemcpy( & SeedVec[0] , Pong , BufferSize , cudaMemcpyDeviceToHost ) ;

    //
    cudaFree( SiteArray ) ;
    cudaFree( Ping ) ;
    cudaFree( Pong ) ;
    cudaFree( Mutex ) ;

    //
    //
    FILE * Output = fopen( "1.ppm" , "wb" ) ;
    fprintf( Output , "P6\n%d %d\n255\n" , Size , Size ) ;

    std::vector< uchar3 > Pixels( Size * Size ) ;
    for ( int y = 0 ; y < Size ; ++ y )
    {
        for ( int x = 0 ; x < Size ; ++ x )
        {
            const int Seed = SeedVec[x + y * Size] ;
            if ( Seed != - 1 )
            {
                Pixels[x + y * Size] = RandomColorVec[Seed] ;
            }
        }
    }

    for( std::vector< float2 >::const_iterator itr = SiteVec.begin() ; itr != SiteVec.end() ; ++ itr )
    {
        const int x = static_cast< int >( floorf( itr->x ) ) ;
        const int y = static_cast< int >( floorf( itr->y ) ) ;
        Pixels[x + y * Size] = make_uchar3( 255 , 0 , 0 ) ;
    }

    fwrite( & Pixels[0].x , 3 , Pixels.size() , Output ) ;
    fclose( Output ) ;
}