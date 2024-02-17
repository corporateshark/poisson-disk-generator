/**
 * \file Poisson.cpp
 * \brief
 *
 * Poisson Disk Points Generator example
 *
 * \version 1.6.1
 * \date 16/02/2024
 * \author Sergey Kosarevsky, 2014-2024
 * \author support@linderdaum.com   http://www.linderdaum.com   http://blog.linderdaum.com
 */

/*
   To compile:
      gcc Poisson.cpp -std=c++17 -lstdc++
*/

#include <fstream>
#include <iomanip>
#include <iostream>
#include <math.h>
#include <memory.h>
#include <vector>

#define POISSON_PROGRESS_INDICATOR 1
#include "PoissonGenerator.h"

#include "argh/argh.h"

///////////////// User selectable parameters ///////////////////////////////

const int kNumPointsDefaultPoisson = 20000; // default number of points to generate for Poisson disk
const int kNumPointsDefaultVogel = 2000; // default number of points to generate for Vogel disk
const int kNumPointsDefaultJittered = 2500; // default number of points to generate for jittered grid
const int kImageSize = 512; // generate RGB image [ImageSize x ImageSize]

////////////////////////////////////////////////////////////////////////////

float* g_DensityMap = nullptr;

#if defined(__GNUC__)
#define GCC_PACK(n) __attribute__((packed, aligned(n)))
#else
#define GCC_PACK(n) __declspec(align(n))
#endif // __GNUC__

#pragma pack(push, 1)
struct GCC_PACK(1) sBMPHeader {
  // BITMAPFILEHEADER
  unsigned short bfType;
  uint32_t bfSize;
  unsigned short bfReserved1;
  unsigned short bfReserved2;
  uint32_t bfOffBits;
  // BITMAPINFOHEADER
  uint32_t biSize;
  uint32_t biWidth;
  uint32_t biHeight;
  unsigned short biPlanes;
  unsigned short biBitCount;
  uint32_t biCompression;
  uint32_t biSizeImage;
  uint32_t biXPelsPerMeter;
  uint32_t biYPelsPerMeter;
  uint32_t biClrUsed;
  uint32_t biClrImportant;
};
#pragma pack(pop)

void SaveBMP(const char* FileName, const void* RawBGRImage, int Width, int Height) {
  sBMPHeader Header;

  int ImageSize = Width * Height * 3;

  Header.bfType = 0x4D * 256 + 0x42;
  Header.bfSize = ImageSize + sizeof(sBMPHeader);
  Header.bfReserved1 = 0;
  Header.bfReserved2 = 0;
  Header.bfOffBits = 0x36;
  Header.biSize = 40;
  Header.biWidth = Width;
  Header.biHeight = Height;
  Header.biPlanes = 1;
  Header.biBitCount = 24;
  Header.biCompression = 0;
  Header.biSizeImage = ImageSize;
  Header.biXPelsPerMeter = 6000;
  Header.biYPelsPerMeter = 6000;
  Header.biClrUsed = 0;
  Header.biClrImportant = 0;

  std::ofstream File(FileName, std::ios::out | std::ios::binary);

  File.write((const char*)&Header, sizeof(Header));
  File.write((const char*)RawBGRImage, ImageSize);

  std::cout << "Saved " << FileName << std::endl;
}

unsigned char* LoadBMP(const char* FileName, int* OutWidth, int* OutHeight) {
  sBMPHeader Header;

  std::ifstream File(FileName, std::ifstream::binary);

  File.read((char*)&Header, sizeof(Header));

  *OutWidth = Header.biWidth;
  *OutHeight = Header.biHeight;

  const size_t DataSize = 3 * Header.biWidth * Header.biHeight;

  unsigned char* Img = new unsigned char[DataSize];

  File.read((char*)Img, DataSize);

  return Img;
}

void LoadDensityMap(const char* FileName) {
  std::cout << "Loading density map " << FileName << std::endl;

  int W, H;
  unsigned char* Data = LoadBMP(FileName, &W, &H);

  std::cout << "Loaded ( " << W << " x " << H << " ) " << std::endl;

  if (W != kImageSize || H != kImageSize) {
    std::cout << "ERROR: density map should be " << kImageSize << " x " << kImageSize << std::endl;

    exit(255);
  }

  g_DensityMap = new float[W * H];

  for (int y = 0; y != H; y++) {
    for (int x = 0; x != W; x++) {
      g_DensityMap[x + y * W] = float(Data[3 * (x + y * W)]) / 255.0f;
    }
  }

  delete[] (Data);
}

void PrintBanner() {
  std::cout << "Poisson disk points generator" << std::endl;
  std::cout << "Version " << PoissonGenerator::Version << std::endl;
  std::cout << "Sergey Kosarevsky, 2014-2023" << std::endl;
  std::cout << "support@linderdaum.com http://www.linderdaum.com http://blog.linderdaum.com" << std::endl;
  std::cout << std::endl;
  std::cout << "Usage: Poisson [density-map-rgb24.bmp] [--raw-points] [--num-points=<value>] [--square] [--vogel-disk | --jittered-grid | "
               "--hammersley]"
            << std::endl;
  std::cout << std::endl;
}

int main(int argc, char** argv) {
  PrintBanner();

  argh::parser cmdl(argv);

  if (!cmdl[1].empty()) {
    LoadDensityMap(cmdl[1].c_str());
  }

  const bool cmdRawPointsOutput = cmdl[{"--raw-points"}];
  const bool cmdSquare = cmdl[{"--square"}];
  const bool cmdVogelDisk = cmdl[{"--vogel-disk"}];
  const bool cmdJitteredGrid = cmdl[{"--jittered-grid"}];
  ;
  const bool cmdHammersley = cmdl[{"--hammersley"}];
  ;

  unsigned int numPoints;
  cmdl("num-points", cmdVogelDisk ? kNumPointsDefaultVogel : (cmdJitteredGrid ? kNumPointsDefaultJittered : kNumPointsDefaultPoisson)) >>
      numPoints;

  std::cout << "NumPoints = " << numPoints << std::endl;

  PoissonGenerator::DefaultPRNG PRNG;

  const auto Points = cmdVogelDisk      ? PoissonGenerator::generateVogelPoints(numPoints)
                      : cmdJitteredGrid ? PoissonGenerator::generateJitteredGridPoints(numPoints, PRNG, !cmdSquare)
                      : cmdHammersley   ? PoissonGenerator::generateHammersleyPoints(numPoints)
                                        : PoissonGenerator::generatePoissonPoints(numPoints, PRNG, !cmdSquare);

  // prepare BGR image
  const size_t DataSize = 3 * kImageSize * kImageSize;

  unsigned char* Img = new unsigned char[DataSize];

  memset(Img, 0, DataSize);

  for (auto i = Points.begin(); i != Points.end(); i++) {
    int x = int(i->x * kImageSize);
    int y = int(i->y * kImageSize);
    if (x < 0 || y < 0 || x >= kImageSize || y >= kImageSize)
      continue;
    if (g_DensityMap) {
      // dice
      float R = PRNG.randomFloat();
      float P = g_DensityMap[x + y * kImageSize];
      if (R > P)
        continue;
    }
    int Base = 3 * (x + y * kImageSize);
    Img[Base + 0] = Img[Base + 1] = Img[Base + 2] = 255;
  }

  SaveBMP("Points.bmp", Img, kImageSize, kImageSize);

  delete[] (Img);

  // dump points to a text file
  std::ofstream File("points.txt", std::ios::out);

  if (cmdRawPointsOutput) {
    File << "NumPoints = " << Points.size() << std::endl;

    for (const auto& p : Points) {
      File << p.x << " " << p.y << std::endl;
    }
  } else {
    File << "const vec2 points[" << Points.size() << "]" << std::endl;
    File << "{" << std::endl;
    File << std::fixed << std::setprecision(6);
    for (const auto& p : Points) {
      File << "\tvec2(" << p.x << "f, " << p.y << "f)," << std::endl;
    }
    File << "};" << std::endl;
  }

  return 0;
}
