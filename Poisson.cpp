/**
 * \file Poisson.cpp
 * \brief
 *
 * Poisson Disk Points Generator example
 *
 * \version 1.1.3
 * \date 10/03/2016
 * \author Sergey Kosarevsky, 2014-2016
 * \author support@linderdaum.com   http://www.linderdaum.com   http://blog.linderdaum.com
 */

/*
	To compile:
		gcc Poisson.cpp -std=c++11 -lstdc++
*/

#include <vector>
#include <iostream>
#include <fstream>
#include <memory.h>

#define POISSON_PROGRESS_INDICATOR 1
#include "PoissonGenerator.h"

///////////////// User selectable parameters ///////////////////////////////

const int   NumPoints   = 20000;	// minimal number of points to generate
const int   ImageSize   = 1024;	// generate RGB image [ImageSize x ImageSize]

////////////////////////////////////////////////////////////////////////////

float* g_DensityMap = nullptr;

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
	std::cout << "Version " << PoissonGenerator::Version << std::endl;
	std::cout << "Sergey Kosarevsky, 2014-2016" << std::endl;
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

	PoissonGenerator::DefaultPRNG PRNG;

	const auto Points = PoissonGenerator::GeneratePoissonPoints( NumPoints, PRNG );

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
			float R = PRNG.RandomFloat();
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
