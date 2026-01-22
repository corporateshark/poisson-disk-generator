/**
 * \file Poisson.cpp
 * \brief
 *
 * Poisson Disk Points Generator example
 *
 * \version 1.7.0
 * \date 21/01/2026
 * \author Sergey Kosarevsky, 2014-2026
 * \author support@linderdaum.com   http://www.linderdaum.com   http://blog.linderdaum.com
 */

/*
   To compile:
      gcc Poisson.cpp -std=c++17 -lstdc++
*/

#include <math.h>
#include <memory.h>
#include <string.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>

#define POISSON_PROGRESS_INDICATOR 1
#include "PoissonGenerator.h"

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

///////////////// Uncompressed AVI Writer //////////////////////////////////

// Simple uncompressed AVI writer (grayscale 8-bit, no audio)
// AVI format reference: https://docs.microsoft.com/en-us/windows/win32/directshow/avi-riff-file-reference
class AVIWriter {
 public:
  AVIWriter(const char* fileName, int width, int height, int skipFrames) {
    m_width = width;
    m_height = height;
    m_skipFrames = skipFrames;
    m_frameDataSize = width * height; // grayscale: 1 byte per pixel
    m_rowPadding = (4 - (width) % 4) % 4; // pad each row to 4-byte boundary (BMP/AVI requirement)
    m_paddedRowSize = width + m_rowPadding;
    m_paddedFrameSize = m_paddedRowSize * height;

    std::cout << "\nSaving video to `" << fileName << "`" << std::endl;

    m_file.open(fileName, std::ios::out | std::ios::binary);

    // write placeholder header (will be updated on close)
    writeHeader();
    m_moviStart = static_cast<uint32_t>(m_file.tellp());

    // start 'movi' LIST
    writeChunkHeader("LIST", 0); // size placeholder
    writeFourCC("movi");
  }
  ~AVIWriter() {
    const uint32_t moviEnd = static_cast<uint32_t>(m_file.tellp());
    const uint32_t moviSize = moviEnd - m_moviStart - 8; // exclude LIST header

    writeIndex(); // write index chunk 'idx1'

    const uint32_t fileEnd = static_cast<uint32_t>(m_file.tellp());

    // update movi LIST size
    m_file.seekp(m_moviStart + 4);
    writeUInt32(moviSize + 4); // +4 for 'movi' fourcc

    // update RIFF size
    m_file.seekp(4);
    writeUInt32(fileEnd - 8);

    // update frame count in header
    m_file.seekp(48); // dwTotalFrames in main AVI header
    writeUInt32(m_frameCount);

    m_file.seekp(140); // dwLength in stream header
    writeUInt32(m_frameCount);

    m_file.close();

    std::cout << "\nSaved AVI with " << m_frameCount << " frames" << std::endl;
  }

  bool addFrame(const void* bgrData, bool isLastFrame) {
    if (!isLastFrame && (m_inputFrameCount++ % m_skipFrames) != 0)
      return false;

    writeChunkHeader("00db", m_paddedFrameSize); // write frame chunk: '00db' for uncompressed DIB

    // convert BGR to grayscale and write with row padding
    const unsigned char* src = static_cast<const unsigned char*>(bgrData);
    std::vector<unsigned char> rowBuffer(m_paddedRowSize, 0);

    for (int y = 0; y < m_height; y++) {
      for (int x = 0; x < m_width; x++) {
        rowBuffer[x] = src[(y * m_width + x) * 3]; // in our grayscale case, just take the first channel
      }
      m_file.write(reinterpret_cast<const char*>(rowBuffer.data()), m_paddedRowSize);
    }

    m_frameCount++;

    return true;
  }

 private:
  std::ofstream m_file;
  int m_width = 0;
  int m_height = 0;
  int m_fps = 60;
  int m_skipFrames = 16;
  uint32_t m_inputFrameCount = 0;
  uint32_t m_frameCount = 0;
  uint32_t m_frameDataSize = 0;
  uint32_t m_moviStart = 0;
  int m_rowPadding = 0;
  int m_paddedRowSize = 0;
  uint32_t m_paddedFrameSize = 0;

  void writeFourCC(const char* fourcc) {
    m_file.write(fourcc, 4);
  }

  void writeUInt32(uint32_t value) {
    m_file.write(reinterpret_cast<const char*>(&value), 4);
  }

  void writeUInt16(uint16_t value) {
    m_file.write(reinterpret_cast<const char*>(&value), 2);
  }

  void writeChunkHeader(const char* fourcc, uint32_t size) {
    writeFourCC(fourcc);
    writeUInt32(size);
  }

  void writeHeader() {
    writeFourCC("RIFF");
    writeUInt32(0); // file size placeholder
    writeFourCC("AVI ");
    writeFourCC("LIST"); // hdrl LIST
    uint32_t hdrlSizePos = static_cast<uint32_t>(m_file.tellp());
    writeUInt32(0); // size placeholder
    writeFourCC("hdrl");
    // main AVI header (avih)
    writeFourCC("avih");
    writeUInt32(56); // header size
    writeUInt32(1000000 / m_fps); // dwMicroSecPerFrame
    writeUInt32(m_paddedFrameSize * m_fps); // dwMaxBytesPerSec
    writeUInt32(0); // dwPaddingGranularity
    writeUInt32(0x10); // dwFlags (AVIF_HASINDEX)
    writeUInt32(0); // dwTotalFrames (placeholder)
    writeUInt32(0); // dwInitialFrames
    writeUInt32(1); // dwStreams
    writeUInt32(m_paddedFrameSize); // dwSuggestedBufferSize
    writeUInt32(m_width); // dwWidth
    writeUInt32(m_height); // dwHeight
    writeUInt32(0); // dwReserved[4]
    writeUInt32(0);
    writeUInt32(0);
    writeUInt32(0);
    // stream LIST
    writeFourCC("LIST");
    uint32_t strlSizePos = static_cast<uint32_t>(m_file.tellp());
    writeUInt32(0); // size placeholder
    writeFourCC("strl");
    // stream header (strh)
    writeFourCC("strh");
    writeUInt32(56); // header size
    writeFourCC("vids"); // fccType
    writeFourCC("DIB "); // fccHandler (uncompressed)
    writeUInt32(0); // dwFlags
    writeUInt16(0); // wPriority
    writeUInt16(0); // wLanguage
    writeUInt32(0); // dwInitialFrames
    writeUInt32(1); // dwScale
    writeUInt32(m_fps); // dwRate
    writeUInt32(0); // dwStart
    writeUInt32(0); // dwLength (placeholder)
    writeUInt32(m_paddedFrameSize); // dwSuggestedBufferSize
    writeUInt32(0xFFFFFFFF); // dwQuality
    writeUInt32(0); // dwSampleSize
    writeUInt16(0); // rcFrame left
    writeUInt16(0); // rcFrame top
    writeUInt16(static_cast<uint16_t>(m_width)); // rcFrame right
    writeUInt16(static_cast<uint16_t>(m_height)); // rcFrame bottom
    // stream format (strf) - BITMAPINFOHEADER + palette for 8-bit grayscale
    writeFourCC("strf");
    writeUInt32(40 + 256 * 4); // header size + palette size
    writeUInt32(40); // biSize
    writeUInt32(m_width); // biWidth
    writeUInt32(m_height); // biHeight
    writeUInt16(1); // biPlanes
    writeUInt16(8); // biBitCount (8-bit grayscale)
    writeUInt32(0); // biCompression (BI_RGB)
    writeUInt32(m_paddedFrameSize); // biSizeImage
    writeUInt32(0); // biXPelsPerMeter
    writeUInt32(0); // biYPelsPerMeter
    writeUInt32(256); // biClrUsed
    writeUInt32(256); // biClrImportant

    // write grayscale palette (256 entries, BGRA format)
    for (int i = 0; i < 256; i++) {
      const uint8_t gray = static_cast<uint8_t>(i);
      m_file.put(gray); // B
      m_file.put(gray); // G
      m_file.put(gray); // R
      m_file.put(0); // A (reserved)
    }

    // update strl LIST size
    const uint32_t strlEnd = static_cast<uint32_t>(m_file.tellp());
    m_file.seekp(strlSizePos);
    writeUInt32(strlEnd - strlSizePos - 4);
    m_file.seekp(strlEnd);

    // Update hdrl LIST size
    const uint32_t hdrlEnd = static_cast<uint32_t>(m_file.tellp());
    m_file.seekp(hdrlSizePos);
    writeUInt32(hdrlEnd - hdrlSizePos - 4);
    m_file.seekp(hdrlEnd);
  }

  void writeIndex() {
    writeFourCC("idx1");
    writeUInt32(m_frameCount * 16); // index size

    uint32_t offset = 4; // offset from 'movi' to first frame data

    for (uint32_t i = 0; i < m_frameCount; i++) {
      writeFourCC("00db"); // chunk ID
      writeUInt32(0x10); // flags (AVIIF_KEYFRAME)
      writeUInt32(offset); // offset
      writeUInt32(m_paddedFrameSize); // size

      offset += m_paddedFrameSize + 8; // +8 for chunk header
    }
  }
};

///////////////// BMP Functions ////////////////////////////////////////////

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
  std::cout << "Sergey Kosarevsky, 2014-2026" << std::endl;
  std::cout << "support@linderdaum.com http://www.linderdaum.com http://blog.linderdaum.com" << std::endl;
  std::cout << std::endl;
  std::cout << "Usage: Poisson [density-map-rgb24.bmp] [--raw-points] [--num-points=<value>] [--square] [--vogel-disk | --jittered-grid | "
               "--hammersley] [--shuffle] [--save-frames] [--save-video[=<skip-frames>]]"
            << std::endl;
  std::cout << std::endl;
}

int main(int argc, char** argv) {
  PrintBanner();

  if (argc > 1 && !strstr(argv[1], "--")) {
    LoadDensityMap(argv[1]);
  }

  auto hasCmdLineArg = [argc, argv](const char* arg) -> bool {
    for (int i = 1; i < argc; i++) {
      if (!strcmp(argv[i], arg)) {
        return true;
      }
    }
    return false;
  };

  auto getCmdLineValue = [argc, argv](const char* arg, unsigned int defaultValue) -> unsigned int {
    for (int i = 1; i < argc; i++) {
      if (strstr(argv[i], arg)) {
        unsigned int v = defaultValue;
        return sscanf(argv[i], "--num-points=%u", &v) == 1 ? v : defaultValue;
      }
    }
    return defaultValue;
  };

  auto getCmdLineValueSkipFrames = [argc, argv](const char* arg, unsigned int defaultValue) -> unsigned int {
    for (int i = 1; i < argc; i++) {
      if (strstr(argv[i], arg)) {
        unsigned int v = defaultValue;
        if (sscanf(argv[i], "--save-video=%u", &v) == 1) {
          return v;
        }
        return defaultValue;
      }
    }
    return defaultValue;
  };

  auto hasCmdLineArgPrefix = [argc, argv](const char* prefix) -> bool {
    for (int i = 1; i < argc; i++) {
      if (strstr(argv[i], prefix) == argv[i]) {
        return true;
      }
    }
    return false;
  };

  const bool cmdRawPointsOutput = hasCmdLineArg("--raw-points");
  const bool cmdSquare = hasCmdLineArg("--square");
  const bool cmdVogelDisk = hasCmdLineArg("--vogel-disk");
  const bool cmdJitteredGrid = hasCmdLineArg("--jittered-grid");

  const bool cmdHammersley = hasCmdLineArg("--hammersley");

  const bool cmdShuffle = hasCmdLineArg("--shuffle");
  const bool cmdSaveFrames = hasCmdLineArg("--save-frames");
  const bool cmdSaveVideo = hasCmdLineArgPrefix("--save-video");
  const unsigned int videoSkipFrames = getCmdLineValueSkipFrames("--save-video", 16);

  const unsigned int numPoints = getCmdLineValue(
      "--num-points=", cmdVogelDisk ? kNumPointsDefaultVogel : (cmdJitteredGrid ? kNumPointsDefaultJittered : kNumPointsDefaultPoisson));

  std::cout << "NumPoints = " << numPoints << std::endl;

  PoissonGenerator::DefaultPRNG PRNG;

  auto Points = cmdVogelDisk      ? PoissonGenerator::generateVogelPoints(numPoints)
                : cmdJitteredGrid ? PoissonGenerator::generateJitteredGridPoints(numPoints, PRNG, !cmdSquare)
                : cmdHammersley   ? PoissonGenerator::generateHammersleyPoints(numPoints)
                                  : PoissonGenerator::generatePoissonPoints(numPoints, PRNG, !cmdSquare);

  // prepare BGR image
  const size_t DataSize = 3 * kImageSize * kImageSize;

  unsigned char* Img = new unsigned char[DataSize];

  memset(Img, 0, DataSize);

  if (cmdShuffle) {
    std::cout << "Shuffling points..." << std::endl;
    PoissonGenerator::shuffle(Points, PRNG);
  }

  std::unique_ptr<AVIWriter> aviWriter = cmdSaveVideo ? std::make_unique<AVIWriter>("Points.avi", kImageSize, kImageSize, videoSkipFrames)
                                                      : nullptr;

  int frame = 0;
  size_t currentPoint = 0;
  const size_t totalPoints = Points.size();

  for (const auto& i : Points) {
    currentPoint++;
    const int x = int(i.x * kImageSize);
    const int y = int(i.y * kImageSize);
    if (x < 0 || y < 0 || x >= kImageSize || y >= kImageSize)
      continue;
    if (g_DensityMap) {
      // dice
      float R = PRNG.randomFloat();
      float P = g_DensityMap[x + y * kImageSize];
      if (R > P)
        continue;
    }
    const int Base = 3 * (x + y * kImageSize);
    Img[Base + 0] = Img[Base + 1] = Img[Base + 2] = 255;

    if (cmdSaveFrames) {
      char fileName[64] = {};
      snprintf(fileName, sizeof(fileName), "pnt%05i.bmp", frame++);
      SaveBMP(fileName, Img, kImageSize, kImageSize);
    }

    if (aviWriter && aviWriter->addFrame(Img, currentPoint == totalPoints)) {
      std::cout << "\rRendering points to video: " << currentPoint << "/" << totalPoints << std::flush;
    }
  }

  // always flush the final frame to video
  if (aviWriter && aviWriter->addFrame(Img, true)) {
    std::cout << "\rRendering points to video: " << currentPoint << "/" << totalPoints << std::flush;
  }

  aviWriter = nullptr;

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
