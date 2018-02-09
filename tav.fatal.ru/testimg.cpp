// testimg.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "image.h"

#include <iostream>
#include <string>

using namespace std;
#include "hashmap.h"


#include <ft2build.h>
#include FT_FREETYPE_H

template <typename T1, typename T2>
bool PrepareDrawPixels(T1 *&dstFirstRow, int &dstWidth, int &dstHeight, int &dstPitch, int &dstOffX, int &dstOffY,
					   T2 *&srcFirstRow, int &srcWidth, int &srcHeight, int &srcPitch)
{
	if (dstOffX < 0) srcFirstRow -= dstOffX, srcWidth += dstOffX, dstOffX = 0;
	if (dstOffY < 0) srcFirstRow -= dstOffY*srcPitch, srcHeight += dstOffY, dstOffY = 0;
	if (srcWidth  + dstOffX > dstWidth ) srcWidth  = dstWidth  - dstOffX;
	if (srcHeight + dstOffY > dstHeight) srcHeight = dstHeight - dstOffY;

	if (srcWidth <= 0 || srcHeight <= 0) return false;

	dstFirstRow += dstOffX + dstOffY * dstPitch;
	return true;
}

void DrawPixels(Byte *dstFirstRow, int dstWidth, int dstHeight, int dstPitch, int dstOffX, int dstOffY,
				Byte *srcFirstRow, int srcWidth, int srcHeight, int srcPitch)
{
	if (!PrepareDrawPixels(dstFirstRow, dstWidth, dstHeight, dstPitch, dstOffX, dstOffY,
						   srcFirstRow, srcWidth, srcHeight, srcPitch)) return;

	for (; srcHeight; srcHeight--, dstFirstRow += dstPitch, srcFirstRow += srcPitch)
		memcpy(dstFirstRow, srcFirstRow, srcWidth);
}

void DrawPixels(Image *dst, int dstOffX, int dstOffY,
				Byte *srcFirstRow, int srcWidth, int srcHeight, int srcPitch)
{
	DrawPixels((Byte*)dst->pixels, dst->width*sizeof(Pixel), dst->height, dst->width*sizeof(Pixel),
				dstOffX*sizeof(Pixel), dstOffY, srcFirstRow, srcWidth, srcHeight, srcPitch);
}

void DrawPixels(Image8 *dst, int dstOffX, int dstOffY,
				Byte *srcFirstRow, int srcWidth, int srcHeight, int srcPitch)
{
	DrawPixels(dst->pixels, dst->width, dst->height, dst->width, dstOffX, dstOffY,
				srcFirstRow, srcWidth, srcHeight, srcPitch);
}

void DrawPixels(Image *dst, int dstOffX, int dstOffY, Image *src)
{
	DrawPixels((Byte*)dst->pixels, dst->width*sizeof(Pixel), dst->height, dst->width*sizeof(Pixel),
				dstOffX*sizeof(Pixel), dstOffY, (Byte*)src->pixels, src->width*sizeof(Pixel), src->height, src->width*sizeof(Pixel));
}

void DrawPixels(Image8 *dst, int dstOffX, int dstOffY, Image8 *src)
{
	DrawPixels(dst->pixels, dst->width, dst->height, dst->width, dstOffX, dstOffY,
				src->pixels, src->width, src->height, src->width);
}

void DrawPixels(Image *dst, int dstOffX, int dstOffY,
				Byte *srcFirstRow, int srcWidth, int srcHeight, int srcPitch, const Pixel &color)
{
	Pixel *dstFirstRow = dst->pixels;
	int dstWidth = dst->width, dstHeight = dst->height, dstPitch = dstWidth;

	if (!PrepareDrawPixels(dstFirstRow, dstWidth, dstHeight, dstPitch, dstOffX, dstOffY,
						   srcFirstRow, srcWidth, srcHeight, srcPitch)) return;

	for (; srcHeight; srcHeight--, dstFirstRow += dstPitch, srcFirstRow += srcPitch)
		for (int i=0; i<srcWidth; i++)
		{
			dstFirstRow[i].r = unsigned(srcFirstRow[i] * color.r)/256;
			dstFirstRow[i].g = unsigned(srcFirstRow[i] * color.g)/256;
			dstFirstRow[i].b = unsigned(srcFirstRow[i] * color.b)/256;
		}
}

void DrawPixelsMasked(Image *dst, int dstOffX, int dstOffY,
	Byte *srcFirstRow, int srcWidth, int srcHeight, int srcPitch)
{
	Pixel *dstFirstRow = dst->pixels;
	int dstWidth = dst->width, dstHeight = dst->height, dstPitch = dstWidth;
	struct Pixel4 {Byte r,g,b,a;} *srcImg = (Pixel4*)srcFirstRow;

	if (!PrepareDrawPixels(dstFirstRow, dstWidth, dstHeight, dstPitch, dstOffX, dstOffY,
						   srcImg, srcWidth, srcHeight, srcPitch)) return;

	for (; srcHeight; srcHeight--, dstFirstRow += dstPitch, srcFirstRow += srcPitch)
		for (int i=0; i<srcWidth; i++)
			if (srcImg[i].a) dstFirstRow[i] = (Pixel&)srcImg[i];
}

#include <Windows.h>

COLORREF *GetScreenArea(int left, int top, int right, int bottom)
{
	static COLORREF *buffer = NULL;
	static int bufferSize = 0;

	int width = right - left, height = bottom - top;

	if (bufferSize < width * height)
	{
		if (buffer) delete [] buffer;
		buffer = new COLORREF [bufferSize = width * height];
	}

	HDC sdc = GetDC(NULL), dc = CreateCompatibleDC(sdc);
	HBITMAP bmp = CreateCompatibleBitmap(sdc, width, height);
	SelectObject(dc, bmp);
	BitBlt(dc, 0, 0, width, height, sdc, left, top, SRCCOPY);

	BITMAPINFO bi;
	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
	bi.bmiHeader.biWidth = width;
	bi.bmiHeader.biHeight = -height;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	bi.bmiHeader.biSizeImage = width * height * sizeof(COLORREF);
	bi.bmiHeader.biClrUsed = 0;
	bi.bmiHeader.biClrImportant = 0;

	if (GetDIBits(dc, bmp, 0, height, buffer, &bi, DIB_RGB_COLORS) < height) return NULL;
	DeleteObject(bmp);
	DeleteDC(dc);
	ReleaseDC(NULL, sdc);

	return buffer;
}

int _tmain(int argc, _TCHAR* argv[])
{
	for (int i=0;i<1000000;i++) cout<<i<<'-'<<GetScreenArea(100,700,300,800)<<endl;
	{

	Image *im = CreateImage(200,100);
	memcpy(im->pixels,GetScreenArea(100,700,300,800),200*100*3);
		void *dst = malloc(1000000);
		int sz=im->writePNG(dst,1000000);
		FILE*f=fopen("scr.png","wb");
		fwrite(dst,1,sz,f);
		fclose(f);
	return 0;
	}

	void *p=malloc(100000);
	FILE *f=fopen("untitled.bmp","rb");
	int ss=fread(p,1,100000,f);
	fclose(f);
	Image *img = CreateImage(64,32);
	memset(img->pixels,0,sizeof(Pixel)*64*32);
	Image *img2 = CreateImage(p,ss);

//	DrawPixels(img,-4,-5,img2);
	FT_Library library;
	FT_Init_FreeType( &library );
	FT_Face face;
	FT_New_Face( library, "C:/windows/fonts/tahoma.ttf", 0, &face );
	FT_Set_Pixel_Sizes( face, 0, 18 );

	FT_GlyphSlot  slot = face->glyph;
	int           pen_x, pen_y, n;

	pen_x = 10;
	pen_y = 16;
	Pixel px = {200,20,20};

	for ( char *p = "|text,"; *p ; p++ )
	{
		if ( FT_Load_Glyph( face, FT_Get_Char_Index( face, *p ), FT_LOAD_DEFAULT ) ) continue;
		if ( FT_Render_Glyph( face->glyph, FT_RENDER_MODE_NORMAL ) ) continue;

		DrawPixels( img, pen_x + slot->bitmap_left, pen_y - slot->bitmap_top,
			slot->bitmap.buffer, slot->bitmap.width, slot->bitmap.rows, slot->bitmap.pitch, px );

		pen_x += slot->advance.x >> 6;
	}

	FT_Done_Face( face );
	FT_Done_FreeType( library );

	void *dst = malloc(100000);
	int sz=img->writePNG(dst,100000);
	f=fopen("te.png","wb");
	fwrite(dst,1,sz,f);
	fclose(f);

	//for  (int i=0;i<1000000;i++)
/*	{
		StorageHashMap<2,string,int> hash;
		hash.add("ff")=4;
		hash.add("fff")=50;
		hash.add("ee9")=6;
		hash.add("ff7")=5;
		hash.add("g5")=57;
		for (StorageHashMap<2,string,int>::Iterator p=hash.begin(); p!=hash.end(); p++)
		//	cout<<p->key<<'='<<*hash[(*p).key]<<endl;
		;
	}
*/
	struct
	{
		int w,h;
		Pixel data[8];
	} ii={4,2,255,0,0, 254,0,0, 252,100,0, 0,0,0
			 ,255,0,0, 254,0,0, 253,0,0, 253,0,0};

	Pixel cm[1]={252,80,0};
	Image8 *i = ((Image*)&ii)->quantize(3,1,cm);
	/*for (int i=0;i<1000000;i++)
	{
		FILE *f = fopen("pic3.bmp","rb");
		fseek(f,0,SEEK_END);
		int size = ftell(f);
		rewind(f);
		void *src = malloc(size);
		fread(src,size,1,f);
		fclose(f);
		Image *img = CreateImage(src,size);
		free(src);
		if (img) free(img);
	}*/

	return 0;
}

