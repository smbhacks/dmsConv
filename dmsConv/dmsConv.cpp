#include <iostream>
#include <fstream>
#include <string>
#include <format>
#include <filesystem>

#include "bmp.h"

char TRANSPARENT_RGB[] = { 0x00, 0x01 };

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
	16-bit file bit value [fileNumColorBits]
		-> 0x10 0x00: 16-bit RGB565
		-> 0x08 0x00: 8-bit palette indexed?
	32-bit layer count (number of sprites/images in the file) [fileNumImages]
	32 bytes .wav filename [fileWavName]
	32 bytes .dms filename (why?) [fileDmsName]

	IMAGE HEADER
	for 16-bit images:
		12 bytes unknown
		32-bit image width
		32-bit image height
		88 bytes unknown
		32-bit image data size / 2
		8 bytes unknown

	for 8-bit images:
		todo

	LINE DATA
	16-bit number of segments in current line of image

	SEGMENT DATA
	16-bit number of completely transparent pixels around the segment
	16-bit number of color values (RGB565 or palette index)
	<previously mentioned number of color values>
	*/

	uint16_t fileNumColorBits;
	uint32_t fileNumImages;
	char fileWavName[32];
	char fileDmsName[32];

	input.seekg(0, std::ios::beg);

	// read file header
	input.ignore(2);
	input.read(reinterpret_cast<char*>(&fileNumColorBits), sizeof fileNumColorBits);
	input.read(reinterpret_cast<char*>(&fileNumImages), sizeof fileNumImages);
	input.read(fileWavName, 32);
	input.read(fileDmsName, 32);

	// log
	std::cout << std::format("\nNumber of color bits: {}", fileNumColorBits);
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

		bmp BMPFile(std::format("{}_{}.bmp", filename, curImage), imageWidth, imageHeight);

		uint64_t pixelsCopiedAll = 0;
		size_t numOfTotalSegments = 0;
		size_t bytesRead = 0;
		while (bytesRead < imageDataSize * 2)
		{
			numOfTotalSegments++;
			uint32_t pixelsCopiedinLine = 0;

			uint16_t lineNumSegments;
			input.read(reinterpret_cast<char*>(&lineNumSegments), sizeof lineNumSegments);
			bytesRead += sizeof lineNumSegments;
			std::cout << std::format("\n\tSegments in line: {} [read {}]", lineNumSegments, bytesRead);

			for (uint16_t curSegment = 0; curSegment < lineNumSegments; curSegment++)
			{
				uint16_t segNumTransparentPx;
				uint16_t segNumColorBytes;
				input.read(reinterpret_cast<char*>(&segNumTransparentPx), sizeof segNumTransparentPx);
				input.read(reinterpret_cast<char*>(&segNumColorBytes), sizeof segNumColorBytes);
				bytesRead += sizeof segNumTransparentPx + sizeof segNumColorBytes;

				// log
				std::cout << std::format("\n\t\tSegment {}:", curSegment);
				std::cout << std::format("\n\t\t\tNumber of surrounding transparent pixels: {}", segNumTransparentPx);
				std::cout << std::format("\n\t\t\tNumber of color bytes: {}", segNumColorBytes);
				std::cout << std::format("\n\t\t\t[Read {}]", bytesRead);

				for (uint16_t i = 0; i < segNumTransparentPx; i++)
					BMPFile.writePixel(TRANSPARENT_RGB);
				for (uint16_t i = 0; i < segNumColorBytes; i++)
				{
					char RGB565[2];
					input.read(RGB565, 2);
					if (input.fail())
					{
						std::cout << "ERROR_READ";
					}
					bool eof = input.eof();
					bool good = input.good();
					bytesRead += 2;
					BMPFile.writePixel(RGB565);

					pixelsCopiedinLine++;
					pixelsCopiedAll++;
					//std::cout << std::format("\n\t\t\t{:2X}{:2X}", (unsigned int)colorVal[0], (unsigned int)colorVal[1]);
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