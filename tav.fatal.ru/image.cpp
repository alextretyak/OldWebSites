#include "common.h"
#include <gif_lib.h>
#include <png.h>
#include <jpeglib.h>
#include <jerror.h>

namespace
{
	const size_t ImageHeaderSize = sizeof(Image().width) + sizeof(Image().height);

	struct MemoryFile
	{
		const Byte *p;
		size_t left;
		jmp_buf setjmp_buffer;
	};


	void PngMemoryReadRoutine(png_structp png_ptr, png_bytep buf, png_size_t length)
	{
		MemoryFile *mf = (MemoryFile*)png_get_io_ptr(png_ptr);

		if (mf->left < length)
			png_error(png_ptr, "EOF reached");
		else
		{
			memcpy(buf, mf->p, length);
			mf->p += length;
			mf->left -= length;
		}
	}

	int GifMemoryReadRoutine(GifFileType *f, GifByteType *buf, int size)
	{
		MemoryFile *mf = (MemoryFile*)f->UserData;

		size_t r = min(size_t(size), mf->left);
		memcpy(buf, mf->p, r);
		mf->p += r;
		mf->left -= r;
		return (int)r;
	}

	void JpegErrorExit(j_common_ptr cinfo)
	{
#ifndef RELEASE
		(*cinfo->err->output_message)(cinfo);
#endif
		longjmp(((MemoryFile*)cinfo->client_data)->setjmp_buffer, 1);
	}

	void JpegInitSource(j_decompress_ptr cinfo)
	{
		MemoryFile *mf = (MemoryFile*)cinfo->client_data;

		cinfo->src->next_input_byte = (JOCTET*)mf->p;
		cinfo->src->bytes_in_buffer = mf->left;
	}

	boolean JpegFillInputBuffer(j_decompress_ptr cinfo)
	{
		return FALSE;
	}

	void JpegSkipInputData(j_decompress_ptr cinfo, long num_bytes)
	{
		if (num_bytes <= 0) return;
		if (cinfo->src->bytes_in_buffer < (size_t)num_bytes) ERREXIT(cinfo, JERR_INPUT_EOF);

		cinfo->src->next_input_byte += (size_t)num_bytes;
		cinfo->src->bytes_in_buffer -= (size_t)num_bytes;
	}

	void JpegTermSource(j_decompress_ptr cinfo)
	{
	}
}

Image *CreateImage(const void *srcImg, size_t srcImgLength)
{
	MemoryFile mf = {(Byte*)srcImg, srcImgLength};
	Image *res = NULL;

	if (srcImgLength >= 8 && png_check_sig((png_byte*)mf.p, 8)) //PNG
	{
		if (png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))
		{
			if (png_infop info_ptr = png_create_info_struct(png_ptr))
			{
				png_bytep *row_pointers = NULL;

				if (setjmp(png_jmpbuf(png_ptr)))
				{
					png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
					if (row_pointers) free(row_pointers);
					if (res) free(res);
					return NULL;
				}

				png_set_read_fn(png_ptr, &mf, PngMemoryReadRoutine);
				png_read_info(png_ptr, info_ptr);
				png_uint_32 width = 0, height = 0;
				int bit_depth = 0, color_type = 0;
				png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

				if ((row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height)) &&
					(res = (Image*)malloc(ImageHeaderSize + sizeof(Pixel)*width*height)))
				{
					if (bit_depth == 16) png_set_strip_16(png_ptr);
					if (color_type & PNG_COLOR_MASK_ALPHA) png_set_strip_alpha(png_ptr);
					if (bit_depth < 8) png_set_packing(png_ptr);
					if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
					if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_gray_1_2_4_to_8(png_ptr);
					if (color_type == PNG_COLOR_TYPE_GRAY ||
						color_type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png_ptr);

					png_read_update_info(png_ptr, info_ptr);

					if (png_get_rowbytes(png_ptr, info_ptr) == sizeof(Pixel)*width)
					{
						res->width  = width;
						res->height = height;

						for (png_uint_32 i = 0; i < height; i++)
							row_pointers[i] = (png_bytep)&res->pixels[i * width];
						png_read_image(png_ptr, row_pointers);
					}
					else
					{
						free(res);
						res = NULL;
					}

					png_read_end(png_ptr, NULL);
				}

				png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
				if (row_pointers) free(row_pointers);
				return res;
			}

			png_destroy_read_struct(&png_ptr, NULL, NULL);
		}
	}
	else if (GifFileType *f = DGifOpen(&mf, GifMemoryReadRoutine)) //GIF
	{
		size_t pcount = f->SWidth * f->SHeight;
		GifPixelType *img8 = (GifPixelType*)malloc(pcount);
		if (img8 == NULL) { DGifCloseFile(f); return NULL; }

		memset(img8, f->SBackGroundColor, pcount);

		GifRecordType recordType;
		do
		{
			if (DGifGetRecordType(f, &recordType) != GIF_OK) break;

			if (recordType == EXTENSION_RECORD_TYPE)
			{
				int extCode;
				GifByteType *extension;

				if (DGifGetExtension(f, &extCode, &extension) != GIF_OK) break;

				while (extension)
					if (DGifGetExtensionNext(f, &extension) != GIF_OK) break;

				if (extension) break;
			}
			else if (recordType == IMAGE_DESC_RECORD_TYPE) //просто берём первое попавшееся изображение и всё
			{
				if (DGifGetImageDesc(f) != GIF_OK) break;

				if (f->Image.Left + f->Image.Width > f->SWidth ||
					f->Image.Top + f->Image.Height > f->SHeight) break;

				ColorMapObject *colorMap;
				if (f->Image.ColorMap) colorMap = f->Image.ColorMap;
				else if (f->SColorMap) colorMap = f->SColorMap;
				else break;

				GifPixelType *start = img8 + f->Image.Top*f->SWidth + f->Image.Left,
							 *end = start + f->Image.Height*f->SWidth, *p;

				if (f->Image.Interlace)
				{
					const short InterlacedOffset[] = {0, 4, 2, 1};
					const short InterlacedJumps[] = {8, 8, 4, 2};

					for (int pass = 0; pass < 4; pass++)
					{
						p = start + InterlacedOffset[pass]*f->SWidth;
						for (; p < end; p += InterlacedJumps[pass]*f->SWidth)
							if (DGifGetLine(f, p, f->Image.Width) != GIF_OK) break;

						if (p < end) break;
					}
				}
				else
					for (p = start; p < end; p += f->SWidth)
						if (DGifGetLine(f, p, f->Image.Width) != GIF_OK) break;

				if (p < end) break;

				res = (Image*)malloc(ImageHeaderSize + sizeof(Pixel) * pcount);
				if (res == NULL) break;

				res->width  = f->SWidth;
				res->height = f->SHeight;

				for (size_t i = 0; i < pcount; i++)
					res->pixels[i] = (Pixel&)colorMap->Colors[ img8[i] ];

				free(img8);
				return DGifCloseFile(f) == GIF_OK ? res : (free(res), (Image*)NULL);
			}
		} while (recordType != TERMINATE_RECORD_TYPE && mf.left);

		if (img8) free(img8);
		DGifCloseFile(f);
	}
	else if (srcImgLength >= 2 && *(short*)srcImg == 'B' + ('M'<<8)) while (true) //BMP
	{
		typedef unsigned char BYTE;
		typedef unsigned short WORD;
		typedef unsigned long DWORD;
		typedef long LONG;

#pragma pack(push, 2)
		struct BmpFileHeader
		{
			WORD    bfType;
			DWORD   bfSize;
			WORD    bfReserved1;
			WORD    bfReserved2;
			DWORD   bfOffBits;
		};// *fileHdr = (BmpFileHeader*)srcImg;
#pragma pack(pop)

		if (srcImgLength < sizeof(BmpFileHeader)) break; //недостаточно места для структуры BmpFileHeader

		struct BmpInfoHeader
		{
			DWORD      biSize;
			LONG       biWidth;
			LONG       biHeight;
			WORD       biPlanes;
			WORD       biBitCount;
			DWORD      biCompression;
			DWORD      biSizeImage;
			LONG       biXPelsPerMeter;
			LONG       biYPelsPerMeter;
			DWORD      biClrUsed;
			DWORD      biClrImportant;
		} *infoHdr = (BmpInfoHeader*)((Byte*)srcImg + sizeof(BmpFileHeader));

		if (srcImgLength < (size_t)((Byte*)(infoHdr+1) - (Byte*)srcImg)) break; //недостаточно места для структуры BmpInfoHeader

		Byte *data = (Byte*)((Byte*)infoHdr + infoHdr->biSize);

		if (srcImgLength < (size_t)(data - (Byte*)srcImg) ||
			infoHdr->biWidth < 0 || infoHdr->biCompression != 0) break; //only uncompressed DIB-images supported

		DWORD clrUsed = infoHdr->biClrUsed;
		if (infoHdr->biBitCount < 16 && clrUsed == 0) clrUsed = (2 << infoHdr->biBitCount); // корректируем число используемых цветов

		struct RGBQuad
		{
			BYTE    rgbBlue;
			BYTE    rgbGreen;
			BYTE    rgbRed;
			BYTE    rgbReserved;
		} *colorMap = (RGBQuad*)data;

		data += sizeof(RGBQuad) * clrUsed;
		unsigned long long rowLength = infoHdr->biBitCount/8 * infoHdr->biWidth, rowInc = 0;
		if (rowLength % 4) rowLength += rowInc = 4 - rowLength % 4;

		bool topDown = infoHdr->biHeight < 0;
		if (topDown) infoHdr->biHeight = -infoHdr->biHeight;

		if (srcImgLength < (data - (Byte*)srcImg) + rowLength*infoHdr->biHeight) break;

		if (!(res = (Image*)malloc(ImageHeaderSize + sizeof(Pixel)*infoHdr->biWidth*infoHdr->biHeight))) break;

		LONG h = infoHdr->biHeight;
		Pixel *px = res->pixels;
		if (!topDown) data += rowLength*(infoHdr->biHeight-1), rowInc -= 2*rowLength;

		if (infoHdr->biBitCount == 8)
		{
			for (; h > 0; h--, data += rowInc)
				for (LONG i = infoHdr->biWidth; i; i--, data++, px++)
				{
					px->r = colorMap[*data].rgbRed;
					px->g = colorMap[*data].rgbGreen;
					px->b = colorMap[*data].rgbBlue;
				}
		}
		else if (infoHdr->biBitCount == 16)
		{
			for (; h > 0; h--, data += rowInc)
				for (LONG i = infoHdr->biWidth; i; i--, data+=2, px++)
				{
					WORD p = *(WORD*)data;
					px->r = p>>10 & 0x1F;
					px->g = p>> 5 & 0x1F;
					px->b = p     & 0x1F;
				}
		}
		else if (infoHdr->biBitCount == 24 || infoHdr->biBitCount == 32)
		{
			for (size_t dataInc = infoHdr->biBitCount == 24 ? 3 : 4; h > 0; h--, data += rowInc)
				for (LONG i = infoHdr->biWidth; i; i--, data += dataInc, px++)
				{
					px->r = data[2];
					px->g = data[1];
					px->b = data[0];
				}
		}
		else break;

		res->width  = infoHdr->biWidth;
		res->height = infoHdr->biHeight;

		return res;
	}
	else //try JPEG
	{
		JSAMPARRAY row_pointers = NULL;

		jpeg_decompress_struct cinfo;
		cinfo.client_data = &mf;

		jpeg_error_mgr jerr;
		cinfo.err = jpeg_std_error(&jerr);
		jerr.error_exit = JpegErrorExit;
		if (setjmp(mf.setjmp_buffer))
		{
			jpeg_destroy_decompress(&cinfo);
			if (row_pointers) free(row_pointers);
			if (res) free(res);
			return NULL;
		}

		jpeg_create_decompress(&cinfo);

		jpeg_source_mgr src_mgr;
		src_mgr.init_source = JpegInitSource;
		src_mgr.fill_input_buffer = JpegFillInputBuffer;
		src_mgr.skip_input_data = JpegSkipInputData;
		src_mgr.resync_to_restart = jpeg_resync_to_restart;
		src_mgr.term_source = JpegTermSource;
		cinfo.src = &src_mgr;

		jpeg_read_header(&cinfo, TRUE);

		cinfo.output_components = cinfo.out_color_components = 3;
		cinfo.out_color_space = JCS_RGB;

		jpeg_start_decompress(&cinfo);

		if ((row_pointers = (JSAMPARRAY)malloc(sizeof(JSAMPROW) * cinfo.image_height)) &&
			(res = (Image*)malloc(ImageHeaderSize + sizeof(Pixel)*cinfo.image_width*cinfo.image_height)))
		{
			res->width  = cinfo.image_width;
			res->height = cinfo.image_height;

			for (JDIMENSION i = 0; i < cinfo.image_height; i++)
				row_pointers[i] = (JSAMPROW)&res->pixels[i * cinfo.image_width];

			while (cinfo.output_scanline < cinfo.output_height)
				if (!jpeg_read_scanlines(&cinfo, row_pointers + cinfo.output_scanline, cinfo.image_height - cinfo.output_scanline))
					longjmp(mf.setjmp_buffer, 1);

			jpeg_finish_decompress(&cinfo);
		}

		jpeg_destroy_decompress(&cinfo);
		if (row_pointers) free(row_pointers);
		return res;
	}

	if (res) free(res);
	return NULL;
}

Image *CreateImage(int width, int height)
{
	Image *res = (Image*)malloc(ImageHeaderSize + sizeof(Pixel)*width*height);
	res->width  = width;
	res->height = height;
	return res;
}


namespace
{
	void PngMemoryWriteRoutine(png_structp png_ptr, png_bytep buf, png_size_t length)
	{
		MemoryFile *mf = (MemoryFile*)png_get_io_ptr(png_ptr);

		if (mf->left < length)
			png_error(png_ptr, "Not enough space to write image");
		else
		{
			memcpy((void*)mf->p, buf, length);
			mf->p += length;
			mf->left -= length;
		}
	}

	void PngFlushData(png_structp png_ptr)
	{
	}
}

size_t Image::writePNG(void *dst, size_t dstLength, int comprLevel, bool interlaced)
{
	MemoryFile mf = {(Byte*)dst, dstLength};

	if (png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))
	{
		if (png_infop info_ptr = png_create_info_struct(png_ptr))
		{
			png_bytep *row_pointers = NULL;

			if (setjmp(png_jmpbuf(png_ptr)))
			{
				png_destroy_write_struct(&png_ptr, &info_ptr);
				if (row_pointers) free(row_pointers);
				return 0;
			}

			png_set_write_fn(png_ptr, &mf, PngMemoryWriteRoutine, PngFlushData);
			if (comprLevel != -1) png_set_compression_level(png_ptr, comprLevel);

			png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB,
				interlaced ? PNG_INTERLACE_ADAM7 : PNG_INTERLACE_NONE,
				PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

			if (row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height))
			{
				for (int i = 0; i < height; i++)
					row_pointers[i] = (png_bytep)&pixels[i * width];

				png_set_rows(png_ptr, info_ptr, row_pointers);
				png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
			}
			else mf.left = dstLength;

			png_destroy_write_struct(&png_ptr, &info_ptr);
			if (row_pointers) free(row_pointers);
			return dstLength - mf.left;
		}

		png_destroy_write_struct(&png_ptr, NULL);
	}

	return 0;
}


namespace
{
	void JpegInitDestination(j_compress_ptr cinfo)
	{
		MemoryFile *mf = (MemoryFile*)cinfo->client_data;

		cinfo->dest->next_output_byte = (JOCTET*)mf->p;
		cinfo->dest->free_in_buffer = mf->left;
	}

	boolean JpegEmptyOutputBuffer(j_compress_ptr cinfo)
	{
		return FALSE;
	}

	void JpegTermDestination(j_compress_ptr cinfo)
	{
		((MemoryFile*)cinfo->client_data)->left = cinfo->dest->free_in_buffer;
	}
}

size_t Image::writeJPEG(void *dst, size_t dstLength, int quality, bool optCoding, bool progressive)
{
	MemoryFile mf = {(Byte*)dst, dstLength};
	JSAMPARRAY row_pointers = NULL;

	jpeg_compress_struct cinfo;
	cinfo.client_data = &mf;

	jpeg_error_mgr jerr;
	cinfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = JpegErrorExit;
	if (setjmp(mf.setjmp_buffer))
	{
		jpeg_destroy_compress(&cinfo);
		if (row_pointers) free(row_pointers);
		return 0;
	}

	jpeg_create_compress(&cinfo);

	jpeg_destination_mgr dst_mgr;
	dst_mgr.init_destination = JpegInitDestination;
	dst_mgr.empty_output_buffer = JpegEmptyOutputBuffer;
	dst_mgr.term_destination = JpegTermDestination;
	cinfo.dest = &dst_mgr;

	cinfo.image_width  = width;
	cinfo.image_height = height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);
	if (optCoding) cinfo.optimize_coding = TRUE;
	if (progressive) jpeg_simple_progression(&cinfo);

	jpeg_start_compress(&cinfo, TRUE);

	if (row_pointers = (JSAMPARRAY)malloc(sizeof(JSAMPROW) * height))
	{
		for (JDIMENSION i = 0; i < cinfo.image_height; i++)
			row_pointers[i] = (JSAMPROW)&pixels[i * width];

		while (cinfo.next_scanline < cinfo.image_height)
			if (!jpeg_write_scanlines(&cinfo, row_pointers + cinfo.next_scanline, cinfo.image_height - cinfo.next_scanline))
				longjmp(mf.setjmp_buffer, 1);

		jpeg_finish_compress(&cinfo);
	}
	else mf.left = dstLength;

	jpeg_destroy_compress(&cinfo);
	if (row_pointers) free(row_pointers);
	return dstLength - mf.left;
}


const size_t Image8HeaderSize = sizeof(Image8().width) + sizeof(Image8().height) + sizeof(Image8().colorMap);

#define INDEX(r, g, b)	((r << 10) + (r << 6) + r + (g << 5) + g + b)

#define MAXCOLOR	256

#define FI_RGBA_RED				0
#define FI_RGBA_GREEN			1
#define FI_RGBA_BLUE			2


// Based on C Implementation of Xiaolin Wu's Color Quantizer (v. 2) (see Graphics Gems vol. II, pp. 126-133)

Image8 *Image::quantize(int PaletteSize, int ReserveSize, Pixel *ReservePalette)
{
	struct WuQuantizer
	{
		struct Box
		{
			int r0;			 // min value, exclusive
			int r1;			 // max value, inclusive
			int g0;
			int g1;
			int b0;
			int b1;
			int vol;
		};

		typedef unsigned char BYTE;
		typedef unsigned short WORD;
		typedef unsigned long DWORD;
		typedef long LONG;

		void Hist3D(LONG *vwt, LONG *vmr, LONG *vmg, LONG *vmb, float *m2, int ReserveSize, Pixel *ReservePalette)
		{
			int ind = 0;
			int inr, ing, inb, table[256];
			int i;
			WORD y, x;

			for(i = 0; i < 256; i++)
				table[i] = i * i;

			Pixel *px = img->pixels;

			for(y = 0; y < img->height; y++) {
				for(x = 0; x < img->width; x++, px++)	{
					inr = (px->r >> 3) + 1;
					ing = (px->g >> 3) + 1;
					inb = (px->b >> 3) + 1;
					ind = INDEX(inr, ing, inb);
					Qadd[y*img->width + x] = (WORD)ind;
					// [inr][ing][inb]
					vwt[ind]++;
					vmr[ind] += px->r;
					vmg[ind] += px->g;
					vmb[ind] += px->b;
					m2[ind] += (float)(table[px->r] + table[px->g] + table[px->b]);
				}
			}

			if( ReserveSize > 0 ) {
				int max = 0;
				for(i = 0; i < 35937; i++) {
					if( vwt[ind] > max ) max = vwt[ind];
				}
				max++;
				for(i = 0; i < ReserveSize; i++) {
					inr = (ReservePalette[i].r >> 3) + 1;
					ing = (ReservePalette[i].g >> 3) + 1;
					inb = (ReservePalette[i].b >> 3) + 1;
					ind = INDEX(inr, ing, inb);
					wt[ind] = max;
					mr[ind] = max * ReservePalette[i].r;
					mg[ind] = max * ReservePalette[i].g;
					mb[ind] = max * ReservePalette[i].b;
					gm2[ind] = (float)max * (float)(table[ReservePalette[i].r] + table[ReservePalette[i].g] + table[ReservePalette[i].b]);
				}
			}
		}


		// At conclusion of the histogram step, we can interpret
		// wt[r][g][b] = sum over voxel of P(c)
		// mr[r][g][b] = sum over voxel of r*P(c)  ,  similarly for mg, mb
		// m2[r][g][b] = sum over voxel of c^2*P(c)
		// Actually each of these should be divided by 'ImageSize' to give the usual
		// interpretation of P() as ranging from 0 to 1, but we needn't do that here.


		// We now convert histogram into moments so that we can rapidly calculate
		// the sums of the above quantities over any desired box.

		// Compute cumulative moments
		void M3D(LONG *vwt, LONG *vmr, LONG *vmg, LONG *vmb, float *m2)
		{
			WORD ind1, ind2;
			BYTE i, r, g, b;
			LONG line, line_r, line_g, line_b;
			LONG area[33], area_r[33], area_g[33], area_b[33];
			float line2, area2[33];

			for(r = 1; r <= 32; r++) {
				for(i = 0; i <= 32; i++) {
					area2[i] = 0;
					area[i] = area_r[i] = area_g[i] = area_b[i] = 0;
				}
				for(g = 1; g <= 32; g++) {
					line2 = 0;
					line = line_r = line_g = line_b = 0;
					for(b = 1; b <= 32; b++) {
						ind1 = INDEX(r, g, b); // [r][g][b]
						line += vwt[ind1];
						line_r += vmr[ind1];
						line_g += vmg[ind1];
						line_b += vmb[ind1];
						line2 += m2[ind1];
						area[b] += line;
						area_r[b] += line_r;
						area_g[b] += line_g;
						area_b[b] += line_b;
						area2[b] += line2;
						ind2 = ind1 - 1089; // [r-1][g][b]
						vwt[ind1] = vwt[ind2] + area[b];
						vmr[ind1] = vmr[ind2] + area_r[b];
						vmg[ind1] = vmg[ind2] + area_g[b];
						vmb[ind1] = vmb[ind2] + area_b[b];
						m2[ind1] = m2[ind2] + area2[b];
					}
				}
			}
		}

		// Compute sum over a box of any given statistic
		LONG Vol( Box *cube, LONG *mmt )
		{
			return( mmt[INDEX(cube->r1, cube->g1, cube->b1)]
			- mmt[INDEX(cube->r1, cube->g1, cube->b0)]
			- mmt[INDEX(cube->r1, cube->g0, cube->b1)]
			+ mmt[INDEX(cube->r1, cube->g0, cube->b0)]
			- mmt[INDEX(cube->r0, cube->g1, cube->b1)]
			+ mmt[INDEX(cube->r0, cube->g1, cube->b0)]
			+ mmt[INDEX(cube->r0, cube->g0, cube->b1)]
			- mmt[INDEX(cube->r0, cube->g0, cube->b0)] );
		}

		// The next two routines allow a slightly more efficient calculation
		// of Vol() for a proposed subbox of a given box.  The sum of Top()
		// and Bottom() is the Vol() of a subbox split in the given direction
		// and with the specified new upper bound.


		// Compute part of Vol(cube, mmt) that doesn't depend on r1, g1, or b1
		// (depending on dir)

		LONG Bottom(Box *cube, BYTE dir, LONG *mmt)
		{
			switch(dir)
			{
			case FI_RGBA_RED:
				return( - mmt[INDEX(cube->r0, cube->g1, cube->b1)]
				+ mmt[INDEX(cube->r0, cube->g1, cube->b0)]
				+ mmt[INDEX(cube->r0, cube->g0, cube->b1)]
				- mmt[INDEX(cube->r0, cube->g0, cube->b0)] );
				break;
			case FI_RGBA_GREEN:
				return( - mmt[INDEX(cube->r1, cube->g0, cube->b1)]
				+ mmt[INDEX(cube->r1, cube->g0, cube->b0)]
				+ mmt[INDEX(cube->r0, cube->g0, cube->b1)]
				- mmt[INDEX(cube->r0, cube->g0, cube->b0)] );
				break;
			case FI_RGBA_BLUE:
				return( - mmt[INDEX(cube->r1, cube->g1, cube->b0)]
				+ mmt[INDEX(cube->r1, cube->g0, cube->b0)]
				+ mmt[INDEX(cube->r0, cube->g1, cube->b0)]
				- mmt[INDEX(cube->r0, cube->g0, cube->b0)] );
				break;
			}

			return 0;
		}


		// Compute remainder of Vol(cube, mmt), substituting pos for
		// r1, g1, or b1 (depending on dir)

		LONG Top(Box *cube, BYTE dir, int pos, LONG *mmt)
		{
			switch(dir)
			{
			case FI_RGBA_RED:
				return( mmt[INDEX(pos, cube->g1, cube->b1)]
				-mmt[INDEX(pos, cube->g1, cube->b0)]
				-mmt[INDEX(pos, cube->g0, cube->b1)]
				+mmt[INDEX(pos, cube->g0, cube->b0)] );
				break;
			case FI_RGBA_GREEN:
				return( mmt[INDEX(cube->r1, pos, cube->b1)]
				-mmt[INDEX(cube->r1, pos, cube->b0)]
				-mmt[INDEX(cube->r0, pos, cube->b1)]
				+mmt[INDEX(cube->r0, pos, cube->b0)] );
				break;
			case FI_RGBA_BLUE:
				return( mmt[INDEX(cube->r1, cube->g1, pos)]
				-mmt[INDEX(cube->r1, cube->g0, pos)]
				-mmt[INDEX(cube->r0, cube->g1, pos)]
				+mmt[INDEX(cube->r0, cube->g0, pos)] );
				break;
			}

			return 0;
		}

		// Compute the weighted variance of a box
		// NB: as with the raw statistics, this is really the variance * ImageSize

		float Var(Box *cube)
		{
			float dr = (float) Vol(cube, mr);
			float dg = (float) Vol(cube, mg);
			float db = (float) Vol(cube, mb);
			float xx =  gm2[INDEX(cube->r1, cube->g1, cube->b1)]
			-gm2[INDEX(cube->r1, cube->g1, cube->b0)]
			-gm2[INDEX(cube->r1, cube->g0, cube->b1)]
			+gm2[INDEX(cube->r1, cube->g0, cube->b0)]
			-gm2[INDEX(cube->r0, cube->g1, cube->b1)]
			+gm2[INDEX(cube->r0, cube->g1, cube->b0)]
			+gm2[INDEX(cube->r0, cube->g0, cube->b1)]
			-gm2[INDEX(cube->r0, cube->g0, cube->b0)];

			return (xx - (dr*dr+dg*dg+db*db)/(float)Vol(cube,wt));
		}

		// We want to minimize the sum of the variances of two subboxes.
		// The sum(c^2) terms can be ignored since their sum over both subboxes
		// is the same (the sum for the whole box) no matter where we split.
		// The remaining terms have a minus sign in the variance formula,
		// so we drop the minus sign and MAXIMIZE the sum of the two terms.

		float Maximize(Box *cube, BYTE dir, int first, int last, int *cut, LONG whole_r, LONG whole_g, LONG whole_b, LONG whole_w)
		{
			LONG half_r, half_g, half_b, half_w;
			int i;
			float temp;

			LONG base_r = Bottom(cube, dir, mr);
			LONG base_g = Bottom(cube, dir, mg);
			LONG base_b = Bottom(cube, dir, mb);
			LONG base_w = Bottom(cube, dir, wt);

			float max = 0.0;

			*cut = -1;

			for (i = first; i < last; i++) {
				half_r = base_r + Top(cube, dir, i, mr);
				half_g = base_g + Top(cube, dir, i, mg);
				half_b = base_b + Top(cube, dir, i, mb);
				half_w = base_w + Top(cube, dir, i, wt);

				// now half_x is sum over lower half of box, if split at i

				if (half_w == 0) {		// subbox could be empty of pixels!
					continue;			// never split into an empty box
				} else {
					temp = ((float)half_r*half_r + (float)half_g*half_g + (float)half_b*half_b)/half_w;
				}

				half_r = whole_r - half_r;
				half_g = whole_g - half_g;
				half_b = whole_b - half_b;
				half_w = whole_w - half_w;

				if (half_w == 0) {		// subbox could be empty of pixels!
					continue;			// never split into an empty box
				} else {
					temp += ((float)half_r*half_r + (float)half_g*half_g + (float)half_b*half_b)/half_w;
				}

				if (temp > max) {
					max=temp;
					*cut=i;
				}
			}

			return max;
		}

		bool Cut(Box *set1, Box *set2)
		{
			BYTE dir;
			int cutr, cutg, cutb;

			LONG whole_r = Vol(set1, mr);
			LONG whole_g = Vol(set1, mg);
			LONG whole_b = Vol(set1, mb);
			LONG whole_w = Vol(set1, wt);

			float maxr = Maximize(set1, FI_RGBA_RED, set1->r0+1, set1->r1, &cutr, whole_r, whole_g, whole_b, whole_w);
			float maxg = Maximize(set1, FI_RGBA_GREEN, set1->g0+1, set1->g1, &cutg, whole_r, whole_g, whole_b, whole_w);
			float maxb = Maximize(set1, FI_RGBA_BLUE, set1->b0+1, set1->b1, &cutb, whole_r, whole_g, whole_b, whole_w);

			if ((maxr >= maxg) && (maxr >= maxb)) {
				dir = FI_RGBA_RED;

				if (cutr < 0) {
					return false; // can't split the box
				}
			} else if ((maxg >= maxr) && (maxg>=maxb)) {
				dir = FI_RGBA_GREEN;
			} else {
				dir = FI_RGBA_BLUE;
			}

			set2->r1 = set1->r1;
			set2->g1 = set1->g1;
			set2->b1 = set1->b1;

			switch (dir) {
				case FI_RGBA_RED:
					set2->r0 = set1->r1 = cutr;
					set2->g0 = set1->g0;
					set2->b0 = set1->b0;
					break;

				case FI_RGBA_GREEN:
					set2->g0 = set1->g1 = cutg;
					set2->r0 = set1->r0;
					set2->b0 = set1->b0;
					break;

				case FI_RGBA_BLUE:
					set2->b0 = set1->b1 = cutb;
					set2->r0 = set1->r0;
					set2->g0 = set1->g0;
					break;
			}

			set1->vol = (set1->r1-set1->r0)*(set1->g1-set1->g0)*(set1->b1-set1->b0);
			set2->vol = (set2->r1-set2->r0)*(set2->g1-set2->g0)*(set2->b1-set2->b0);

			return true;
		}


		void Mark(Box *cube, int label, BYTE *tag)
		{
			for (int r = cube->r0 + 1; r <= cube->r1; r++) {
				for (int g = cube->g0 + 1; g <= cube->g1; g++) {
					for (int b = cube->b0 + 1; b <= cube->b1; b++) {
						tag[INDEX(r, g, b)] = (BYTE)label;
					}
				}
			}
		}

		Image8 *Quantize(int PaletteSize, int ReserveSize, Pixel *ReservePalette)
		{
			const int C = 33 * 33 * 33,
				S = C * (sizeof(float) + sizeof(LONG) * 4 + sizeof(BYTE))
					+ sizeof(WORD) * img->width * img->height;

			gm2 = (float*)malloc( S );
			Image8 *new_dib = (Image8*)malloc(Image8HeaderSize + sizeof(Byte)*img->width*img->height);

			if (gm2 && new_dib)
			{
				new_dib->width  = img->width;
				new_dib->height = img->height;

				wt = (LONG*)(gm2 + C);
				mr = (LONG*)(wt + C);
				mg = (LONG*)(mr + C);
				mb = (LONG*)(mg + C);
				tag = (BYTE*)(mb + C);
				Qadd = (WORD*)(tag + C);

				memset(gm2, 0, S);

				Box	cube[MAXCOLOR];
				int	next;
				LONG i, weight;
				int k;
				float vv[MAXCOLOR], temp;

				// Compute 3D histogram

				Hist3D(wt, mr, mg, mb, gm2, ReserveSize, ReservePalette);

				// Compute moments

				M3D(wt, mr, mg, mb, gm2);

				cube[0].r0 = cube[0].g0 = cube[0].b0 = 0;
				cube[0].r1 = cube[0].g1 = cube[0].b1 = 32;
				next = 0;

				for (i = 1; i < PaletteSize; i++) {
					if(Cut(&cube[next], &cube[i])) {
						// volume test ensures we won't try to cut one-cell box
						vv[next] = (cube[next].vol > 1) ? Var(&cube[next]) : 0;
						vv[i] = (cube[i].vol > 1) ? Var(&cube[i]) : 0;
					} else {
						vv[next] = 0.0;   // don't try to split this box again
						i--;              // didn't create box i
					}

					next = 0; temp = vv[0];

					for (k = 1; k <= i; k++) {
						if (vv[k] > temp) {
							temp = vv[k]; next = k;
						}
					}

					if (temp <= 0.0) {
						PaletteSize = i + 1;

						// Error: "Only got 'PaletteSize' boxes"

						break;
					}
				}

				// Partition done

				// create an optimized palette

				Pixel *new_pal = new_dib->colorMap;

				for (k = 0; k < PaletteSize ; k++) {
					Mark(&cube[k], k, tag);
					weight = Vol(&cube[k], wt);

					if (weight) {
						new_pal[k].r = (BYTE)(((float)Vol(&cube[k], mr) / (float)weight) + 0.5f);
						new_pal[k].g = (BYTE)(((float)Vol(&cube[k], mg) / (float)weight) + 0.5f);
						new_pal[k].b = (BYTE)(((float)Vol(&cube[k], mb) / (float)weight) + 0.5f);
					} else {
						// Error: bogus box 'k'

						new_pal[k].r = new_pal[k].g = new_pal[k].b = 0;
					}
				}

				Byte *new_px = new_dib->pixels;

				for (WORD y = 0; y < img->height; y++) {
					for (WORD x = 0; x < img->width; x++, new_px++) {
						*new_px = tag[Qadd[y*img->width + x]];
					}
				}

				free(gm2);
				return new_dib;
			}

			if (gm2) free(gm2);
			if (new_dib) free(new_dib);

			return NULL;
		}

		float *gm2;
		LONG *wt, *mr, *mg, *mb;
		BYTE *tag;
		WORD *Qadd;
		Image *img;

		WuQuantizer(Image *img):img(img) {}
	} q(this);

	return q.Quantize(PaletteSize, ReserveSize, ReservePalette);
}
