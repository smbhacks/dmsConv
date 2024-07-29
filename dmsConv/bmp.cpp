#include "bmp.h"

bmp::bmp(std::string filename, uint32_t imageWidth, uint32_t imageHeight)
    : m_width(imageWidth)
    , m_height(imageHeight)
{
    m_output.open(filename, std::ios::out | std::ios::binary);

    size_t filesize = 54 + 12 + imageWidth * imageHeight * 2;

    int32_t negatedImageHeight = (int32_t)imageHeight * -1;

    unsigned char fileHeader[14] = {
        'B', 'M',             // Signature
        0, 0, 0, 0,           // File size in bytes
        0, 0,                 // Reserved
        0, 0,                 // Reserved
        14 + 40 + 12, 0, 0, 0 // Offset to pixel data
    };

    unsigned char infoHeader[40] = {
        40, 0, 0, 0,          // Header size
        0, 0, 0, 0,           // Image width
        0, 0, 0, 0,           // Image height
        1, 0,                 // Planes
        16, 0,                // Bits per pixel (16 for RGB565)
        3, 0, 0, 0,           // Compression (3 = BI_BITFIELDS)
        0, 0, 0, 0,           // Image size (can be 0 for uncompressed)
        0, 0, 0, 0,           // X pixels per meter
        0, 0, 0, 0,           // Y pixels per meter
        0, 0, 0, 0,           // Total colors (0 = default)
        0, 0, 0, 0            // Important colors (0 = all)
    };

    uint16_t redMask =   0b1111100000000000;
    uint16_t greenMask = 0b0000011111100000;
    uint16_t blueMask =  0b0000000000011111;

    // Set file size in header
    fileHeader[2] = (unsigned char)(filesize);
    fileHeader[3] = (unsigned char)(filesize >> 8);
    fileHeader[4] = (unsigned char)(filesize >> 16);
    fileHeader[5] = (unsigned char)(filesize >> 24);

    // Set image dimensions in info header
    infoHeader[ 4] = (unsigned char)(imageWidth);
    infoHeader[ 5] = (unsigned char)(imageWidth >> 8);
    infoHeader[ 6] = (unsigned char)(imageWidth >> 16);
    infoHeader[ 7] = (unsigned char)(imageWidth >> 24);
    infoHeader[ 8] = (unsigned char)(negatedImageHeight);
    infoHeader[ 9] = (unsigned char)(negatedImageHeight >> 8);
    infoHeader[10] = (unsigned char)(negatedImageHeight >> 16);
    infoHeader[11] = (unsigned char)(negatedImageHeight >> 24);

    m_paddingSize = (4 - ((imageWidth * 2) % 4)) % 4;

    m_output.write(reinterpret_cast<const char*>(fileHeader), sizeof fileHeader);
    m_output.write(reinterpret_cast<const char*>(infoHeader), sizeof infoHeader);

    m_output.write(reinterpret_cast<const char*>(&redMask), 4);
    m_output.write(reinterpret_cast<const char*>(&greenMask), 4);
    m_output.write(reinterpret_cast<const char*>(&blueMask), 4);
}

void bmp::writePixel(char RGB565[])
{
    if (m_Y >= m_height)
    {
        std::cout << "\n[bmp] Overflow error.";
        return;
    }

    m_output.write(RGB565, 2);

    if (m_output.fail() || !m_output.good() || m_output.bad())
    {
        std::cout << "ERROR_WRITE";
    }

    m_X++;
    if (m_X == m_width)
    {
        m_X = 0;
        m_Y++;

        for (int padding = 0; padding < m_paddingSize; padding++)
            m_output.put(0);
    }
}