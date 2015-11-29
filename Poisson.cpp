/**
 * \file Poisson.cpp
 * \brief
 *
 * Poisson Disk Points Generator
 *
 * \version 1.1.2
 * \date 09/04/2015
 * \author Sergey Kosarevsky, 2014-2015
 * \author support@linderdaum.com   http://www.linderdaum.com   http://blog.linderdaum.com
 */

/*
	To compile:
		gcc Poisson.cpp -std=c++11 -lstdc++
*/

// Fast Poisson Disk Sampling in Arbitrary Dimensions
// http://people.cs.ubc.ca/~rbridson/docs/bridson-siggraph07-poissondisk.pdf

// Implementation based on http://devmag.org.za/2009/05/03/poisson-disk-sampling/

/* Versions history:
 *		1.1.2		Apr  9, 2015		Output a text file with XY coordinates
 *		1.1.1		May 23, 2014		Initialize PRNG seed, fixed uninitialized fields
 *    1.1		May  7, 2014		Support of density maps
 *		1.0		May  6, 2014
*/

#include <iostream>
#include <vector>
#include <random>
#include <stdint.h>
#include <time.h>
#include <fstream>
#include <memory.h>

///////////////// User selectable parameters ///////////////////////////////

const int   NumPoints   = 20000;	// minimal number of points to generate
const bool  Circle      = true;	// 'true' to fill a circle, 'false' to fill a rectangle
const int   ImageSize   = 1024;	// generate RGB image [ImageSize x ImageSize]
const int   k           = 30;		// refer to bridson-siggraph07-poissondisk.pdf for details

////////////////////////////////////////////////////////////////////////////

const char* Version = "1.1.2 (09/04/2015)";

const float MinDistance = sqrt( float(NumPoints) ) / float(NumPoints);

std::random_device rd;
std::mt19937 gen( rd() );
std::uniform_real_distribution<> dis( 0.0, 1.0 );

float* g_DensityMap = NULL;

struct sPoint
{
	sPoint()
		: x( 0 )
		, y( 0 )
		, m_Valid( false )
	{}
	sPoint( float X, float Y )
		: x( X )
		, y( Y )
		, m_Valid( true )
	{}
	float x;
	float y;
	bool m_Valid;
	//
	bool IsInRectangle() const
	{
		return x >= 0 && y >= 0 && x <= 1 && y <= 1;
	}
	//
	bool IsInCircle() const
	{
		float fx = x - 0.5f;
		float fy = y - 0.5f;
		return ( fx*fx + fy*fy ) <= 0.25f;
	}
};

struct sGridPoint
{
	sGridPoint( int X, int Y )
		: x( X )
		, y( Y )
	{}
	int x;
	int y;
};

float RandomFloat()
{
	return static_cast<float>( dis( gen ) );
}

float GetDistance( const sPoint& P1, const sPoint& P2 )
{
	return sqrt( ( P1.x - P2.x ) * ( P1.x - P2.x ) + ( P1.y - P2.y ) * ( P1.y - P2.y ) );
}

sGridPoint ImageToGrid( const sPoint& P, float CellSize )
{
	return sGridPoint( ( int )( P.x / CellSize ), ( int )( P.y / CellSize ) );
}

struct sGrid
{
	sGrid( int W, int H, float CellSize )
		: m_W( W )
		, m_H( H )
		, m_CellSize( CellSize )
	{
		m_Grid.resize( m_H );

		for ( auto i = m_Grid.begin(); i != m_Grid.end(); i++ ) { i->resize( m_W ); }
	}
	void Insert( const sPoint& P )
	{
		sGridPoint G = ImageToGrid( P, m_CellSize );
		m_Grid[ G.x ][ G.y ] = P;
	}
	bool IsInNeighbourhood( sPoint Point, float MinDist, float CellSize )
	{
		sGridPoint G = ImageToGrid( Point, CellSize );

		// number of adjucent cells to look for neighbour points
		const int D = 5;

		// scan the neighbourhood of the point in the grid
		for ( int i = G.x - D; i < G.x + D; i++ )
		{
			for ( int j = G.y - D; j < G.y + D; j++ )
			{
				if ( i >= 0 && i < m_W && j >= 0 && j < m_H )
				{
					sPoint P = m_Grid[ i ][ j ];

					if ( P.m_Valid && GetDistance( P, Point ) < MinDist ) { return true; }
				}
			}
		}


		return false;
	}

private:
	int m_W;
	int m_H;
	float m_CellSize;

	std::vector< std::vector<sPoint> > m_Grid;
};

sPoint PopRandom( std::vector<sPoint>& Points )
{
	std::uniform_int_distribution<> dis( 0, Points.size() - 1 );
	int Idx = dis( gen );
	sPoint P = Points[ Idx ];
	Points.erase( Points.begin() + Idx );
	return P;
}

sPoint GenerateRandomPointAround( const sPoint& P, float MinDist )
{
	// start with non-uniform distribution
	float R1 = RandomFloat();
	float R2 = RandomFloat();

	// radius should be between MinDist and 2 * MinDist
	float Radius = MinDist * ( R1 + 1.0f );

	// random angle
	float Angle = 2 * 3.141592653589f * R2;

	// the new point is generated around the point (x, y)
	float X = P.x + Radius * cos( Angle );
	float Y = P.y + Radius * sin( Angle );

	return sPoint( X, Y );
}

std::vector<sPoint> GeneratePoissonPoints( float MinDist, int NewPointsCount, size_t NumPoints )
{
	std::vector<sPoint> SamplePoints;
	std::vector<sPoint> ProcessList;

	// create the grid
	float CellSize = MinDist / sqrt( 2.0f );

	int GridW = ( int )ceil( 1.0f / CellSize );
	int GridH = ( int )ceil( 1.0f / CellSize );

	sGrid Grid( GridW, GridH, CellSize );

	sPoint FirstPoint;
 	do {
		FirstPoint = sPoint( RandomFloat(), RandomFloat() );	     
	} while (!(Circle ? FirstPoint.IsInCircle() : FirstPoint.IsInRectangle()));

	// update containers
	ProcessList.push_back( FirstPoint );
	SamplePoints.push_back( FirstPoint );
	Grid.Insert( FirstPoint );

	// generate new points for each point in the queue
	while ( !ProcessList.empty() && SamplePoints.size() < NumPoints )
	{
		// a progress indicator, kind of
		if ( SamplePoints.size() % 100 == 0 ) std::cout << ".";

		sPoint Point = PopRandom( ProcessList );

		for ( int i = 0; i < NewPointsCount; i++ )
		{
			sPoint NewPoint = GenerateRandomPointAround( Point, MinDist );

			bool Fits = Circle ? NewPoint.IsInCircle() : NewPoint.IsInRectangle();

			if ( Fits && !Grid.IsInNeighbourhood( NewPoint, MinDist, CellSize ) )
			{
				ProcessList.push_back( NewPoint );
				SamplePoints.push_back( NewPoint );
				Grid.Insert( NewPoint );
				continue;
			}
		}
	}

	std::cout << std::endl << std::endl;

	return SamplePoints;
}

#if defined( __GNUC__ )
#	define GCC_PACK(n) __attribute__((packed,aligned(n)))
#else
#	define GCC_PACK(n) __declspec(align(n))
#endif // __GNUC__

#pragma pack(push, 1)
struct GCC_PACK( 1 ) sBMPHeader
{
   // BITMAPFILEHEADER
   unsigned short    bfType;
   uint32_t          bfSize;
   unsigned short    bfReserved1;
   unsigned short    bfReserved2;
   uint32_t          bfOffBits;
   // BITMAPINFOHEADER
   uint32_t          biSize;
   uint32_t          biWidth;
   uint32_t          biHeight;
   unsigned short    biPlanes;
   unsigned short    biBitCount;
   uint32_t          biCompression;
   uint32_t          biSizeImage;
   uint32_t          biXPelsPerMeter;
   uint32_t          biYPelsPerMeter;
   uint32_t          biClrUsed;
   uint32_t          biClrImportant;
};
#pragma pack(pop)

void SaveBMP( const char* FileName, const void* RawBGRImage, int Width, int Height )
{
	sBMPHeader Header;

	int ImageSize = Width * Height * 3;

	Header.bfType          = 0x4D * 256 + 0x42;
	Header.bfSize          = ImageSize + sizeof( sBMPHeader );
	Header.bfReserved1     = 0;
	Header.bfReserved2     = 0;
	Header.bfOffBits       = 0x36;
	Header.biSize          = 40;
	Header.biWidth         = Width;
	Header.biHeight        = Height;
	Header.biPlanes        = 1;
	Header.biBitCount      = 24;
	Header.biCompression   = 0;
	Header.biSizeImage     = ImageSize;
	Header.biXPelsPerMeter = 6000;
	Header.biYPelsPerMeter = 6000;
	Header.biClrUsed       = 0;
	Header.biClrImportant  = 0;

	std::ofstream File( FileName, std::ios::out | std::ios::binary );

	File.write( (const char*)&Header, sizeof( Header ) );
	File.write( (const char*)RawBGRImage, ImageSize );

	std::cout << "Saved " << FileName << std::endl;
}

unsigned char* LoadBMP( const char* FileName, int* OutWidth, int* OutHeight )
{
	sBMPHeader Header;

	std::ifstream File( FileName, std::ifstream::binary );

	File.read( (char*)&Header, sizeof( Header ) );

	*OutWidth  = Header.biWidth;
	*OutHeight = Header.biHeight;

	size_t DataSize = 3 * Header.biWidth * Header.biHeight;

	unsigned char* Img = new unsigned char[ DataSize ];

	File.read( (char*)Img, DataSize );

	return Img;
}

void LoadDensityMap( const char* FileName )
{
	std::cout << "Loading density map " << FileName << std::endl;

	int W, H;
	unsigned char* Data = LoadBMP( FileName, &W, &H );

	std::cout << "Loaded ( " << W << " x " << H << " ) " << std::endl;

	if ( W != ImageSize || H != ImageSize )
	{
		std::cout << "ERROR: density map should be " << ImageSize << " x " << ImageSize << std::endl;		

		exit( 255 );
	}

	g_DensityMap = new float[ W * H ];

	for ( int y = 0; y != H; y++ )
	{
		for ( int x = 0; x != W; x++ )
		{
			g_DensityMap[ x + y * W ] = float( Data[ 3 * (x + y * W) ] ) / 255.0f;
		}
	}

	delete[]( Data );
}

void PrintBanner()
{
	std::cout << "Poisson disk points generator" << std::endl;
	std::cout << "Version " << Version << std::endl;
	std::cout << "Sergey Kosarevsky, 2014" << std::endl;
	std::cout << "support@linderdaum.com http://www.linderdaum.com http://blog.linderdaum.com" << std::endl;
	std::cout << std::endl;
	std::cout << "Usage: Poisson [density-map-rgb24.bmp]" << std::endl;
	std::cout << std::endl;
}

int main( int argc, char** argv )
{
	PrintBanner();

	if ( argc > 1 )
	{
		LoadDensityMap( argv[1] );
	}

	// prepare PRNG
	gen.seed( time( NULL ) );

	std::vector<sPoint> Points = GeneratePoissonPoints( MinDistance, k, NumPoints );

	// prepare BGR image
	size_t DataSize = 3 * ImageSize * ImageSize;

	unsigned char* Img = new unsigned char[ DataSize ];

	memset( Img, 0, DataSize );

	for ( auto i = Points.begin(); i != Points.end(); i++ )
	{
		int x = int( i->x * ImageSize );
		int y = int( i->y * ImageSize );
		if ( g_DensityMap )
		{
			// dice
			float R = RandomFloat();
			float P = g_DensityMap[ x + y * ImageSize ];
			if ( R > P ) continue;
		}
		int Base = 3 * (x + y * ImageSize);
		Img[ Base+0 ] = Img[ Base+1 ] = Img[ Base+2 ] = 255;
	}

	SaveBMP( "Poisson.bmp", Img, ImageSize, ImageSize );

	delete[]( Img );

	// dump points to a text file
	std::ofstream File( "Poisson.txt", std::ios::out );	

	File << "NumPoints = " << Points.size() << std::endl;

	for ( const auto& p : Points )
	{
		File << "X = " << p.x << "; Y = " << p.y << std::endl;
	}

	return 0;
}
