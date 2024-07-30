#include <iostream>
#include <fstream>
#include <string>
#include <format>
#include <filesystem>
#include <array>

#include "bmp.h"

char TRANSPARENT_RGB[] = { 0x01, 0x01, 0x01};

int main(int argc, char* argv[])
{
	std::string ifilename;
	if (argc < 2)
	{
		std::cout << "Enter .dms file: ";
		std::getline(std::cin, ifilename);
	}
	else
		ifilename = argv[2];
	std::ifstream input(ifilename, std::ios::in | std::ios::binary);

	if (!input.is_open())
	{
		std::cout << "Couldn't open the specified file.";
		return -1;
	}

	size_t lastindex = ifilename.find_last_of(".");
	std::string filename = ifilename.substr(0, lastindex);

	/*
	Huge thanks to munjo for figuring out the header
	dms file (little endian)

	DMS FILE HEADER
	2 bytes unknown/dummy
	16-bit file bit value [fileColorType]
		-> 0x10 0x00: 16-bit RGB565
		-> 0x08 0x00: 8-bit palette indexed?
	32-bit layer count (number of sprites/images in the file) [fileNumImages]
	32 bytes .wav filename [fileWavName]
	32 bytes .dms filename (why?) [fileDmsName]

	IMAGE HEADER
		12 bytes unknown
		32-bit image width
		32-bit image height
		88 bytes unknown
		32-bit image data size (*2 if RGB565 mode)
		8 bytes unknown

	LINE DATA (RGB565: 16-bit values, paletteIndexed: 8-bit values)
	1x number of segments in current line of image

	SEGMENT DATA
	1x number of completely transparent pixels before color values
	1x number of color values (RGB565 or palette index)
	<previously mentioned number of color values>
	*/

	uint16_t fileColorType;
	uint32_t fileNumImages;
	char fileWavName[32];
	char fileDmsName[32];

	input.seekg(0, std::ios::beg);

	// read file header
	input.ignore(2);
	input.read(reinterpret_cast<char*>(&fileColorType), sizeof fileColorType);
	input.read(reinterpret_cast<char*>(&fileNumImages), sizeof fileNumImages);
	input.read(fileWavName, 32);
	input.read(fileDmsName, 32);

	// if 8-bits, then there's a palette LUT here (256 * 4 = 1024 bytes, BGR-)
	std::array<char, 256 * 4> filePaletteLUT;
	if (fileColorType == 8)
	{
		input.read(filePaletteLUT.data(), filePaletteLUT.size());
	}

	// log
	std::cout << std::format("\nNumber of color bits: {}", fileColorType);
	std::cout << std::format("\nNumber of images in file: {}", fileNumImages);
	std::cout << std::format("\nAssociated .wav file: {}", fileWavName);

	for (uint32_t curImage = 0; curImage < fileNumImages; curImage++)
	{
		// read image header
		uint32_t imageWidth;
		uint32_t imageHeight;
		uint32_t imageDataSize;

		input.ignore(12);
		input.read(reinterpret_cast<char*>(&imageWidth), sizeof imageWidth);
		input.read(reinterpret_cast<char*>(&imageHeight), sizeof imageHeight);
		input.ignore(88);
		input.read(reinterpret_cast<char*>(&imageDataSize), sizeof imageDataSize);
		input.ignore(8);

		// log
		std::cout << std::format("\nImage {}:", curImage);
		std::cout << std::format("\n\tWidth: {}", imageWidth);
		std::cout << std::format("\n\tHeight: {}", imageHeight);
		std::cout << std::format("\n\tData size: {}", imageDataSize);

		bool is_paletteIndexed = fileColorType == 8;
		bmp BMPFile(std::format("{}_{}.bmp", filename, curImage), imageWidth, imageHeight, is_paletteIndexed);

		uint64_t pixelsCopiedAll = 0;
		size_t numOfTotalSegments = 0;
		size_t bytesRead = 0;
		while ((!is_paletteIndexed && bytesRead < imageDataSize * 2) || (is_paletteIndexed && bytesRead < imageDataSize))
		{
			numOfTotalSegments++;
			uint32_t pixelsCopiedinLine = 0;

			uint8_t readSize = is_paletteIndexed ? 1 : 2;

			uint16_t lineNumSegments = 0;
			input.read(reinterpret_cast<char*>(&lineNumSegments), readSize);
			bytesRead += readSize;
			std::cout << std::format("\n\tSegments in line: {} [read {}]", lineNumSegments, bytesRead);

			for (uint16_t curSegment = 0; curSegment < lineNumSegments; curSegment++)
			{
				uint16_t segNumTransparentPx = 0;
				uint16_t segNumColorBytes = 0;
				input.read(reinterpret_cast<char*>(&segNumTransparentPx), readSize);
				input.read(reinterpret_cast<char*>(&segNumColorBytes), readSize);
				bytesRead += readSize*2;

				// log
				std::cout << std::format("\n\t\tSegment {}:", curSegment);
				std::cout << std::format("\n\t\t\tNumber of preceeding transparent pixels: {}", segNumTransparentPx);
				std::cout << std::format("\n\t\t\tNumber of color bytes: {}", segNumColorBytes);
				std::cout << std::format("\n\t\t\t[Read {}]", bytesRead);

				for (uint16_t i = 0; i < segNumTransparentPx; i++)
					BMPFile.writePixel(TRANSPARENT_RGB);
				for (uint16_t i = 0; i < segNumColorBytes; i++)
				{
					char readVal[2];
					input.read(readVal, readSize);

					if (input.fail())
					{
						std::cout << "ERROR_READ";
					}

					bytesRead += fileColorType / 8;

					if(fileColorType == 16)
						BMPFile.writePixel(readVal);
					else
					{
						uint16_t index = (unsigned char)readVal[0] * 4;
						char paletteValue[3];
						for (int i = 0; i < 3; i++)
						{
							paletteValue[i] = filePaletteLUT[index + i];
						}
						BMPFile.writePixel(paletteValue);
					}

					pixelsCopiedinLine++;
					pixelsCopiedAll++;
				}
				pixelsCopiedinLine += segNumTransparentPx;
				pixelsCopiedAll += segNumTransparentPx;

				std::cout << std::format("\n\t\t\tTransferred pixels [read {}]", bytesRead);
			}
			std::cout << std::format("\n\tPixels copied in this line: {}", pixelsCopiedinLine);
			// handle missing pixels just in case
			if (pixelsCopiedinLine < imageWidth)
			{
				uint32_t diff = imageWidth - pixelsCopiedinLine;
				do
				{
					BMPFile.writePixel(TRANSPARENT_RGB);
					diff--;
				} while (diff > 0);
			}
		}
		std::cout << std::format("\nPixels copied overall: {}", pixelsCopiedAll);
		std::cout << std::format("\nNumber of total segments: {}", numOfTotalSegments);
		// handle rest of image data if missing just in case
		if (pixelsCopiedAll < imageWidth * imageHeight)
		{
			uint64_t diff = imageWidth * imageHeight - pixelsCopiedAll;
			do
			{
				BMPFile.writePixel(TRANSPARENT_RGB);
				diff--;
			} while (diff > 0);
		}
	}

	return 0;
}