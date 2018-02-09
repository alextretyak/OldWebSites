typedef unsigned char Byte;


struct Pixel { Byte r,g,b; };


struct Image8
{
	int width,height;
	Pixel colorMap[256];
	Byte pixels[1];
};


struct Image
{
	int width,height;
	Pixel pixels[1];

	size_t writePNG (void *dst, size_t dstLength, int comprLevel = -1, bool interlaced = false);
	size_t writeJPEG(void *dst, size_t dstLength, int quality, bool optCoding = true, bool progressive = false);
	Image8 *quantize(int PaletteSize = 256, int ReserveSize = 0, Pixel *ReservePalette = NULL);
};

Image *CreateImage(const void *srcImg, size_t srcImgLength);
Image *CreateImage(int width, int height);
