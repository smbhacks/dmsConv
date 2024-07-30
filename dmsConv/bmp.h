#pragma once
#include <iostream>
#include <fstream>

class bmp
{
private:
	std::ofstream m_output;
	int m_paddingSize;
	bool m_is_24bpp;
	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_X = 0;
	uint32_t m_Y = 0;

public:
	bmp(std::string filename, uint32_t imageWidth, uint32_t imageHeight, bool is_24bpp);
	~bmp()
	{
		if (m_output.is_open())
		{
			m_output.close();
		}
	};
	void writePixel(char value[]);
};