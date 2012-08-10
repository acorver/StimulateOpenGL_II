/*
 *  FastMovieFormat.cpp
 *  StimulateOpenGL_II
 *
 *  Created by calin on 8/9/12.
 *  Copyright 2012 Calin Culianu <calin.culianu@gmail.com>. All rights reserved.
 *
 */
#include "FastMovieFormat.h"
#include <stdlib.h>

#ifdef NO_QT
#include <zlib.h>

static void zCompress(const void *inbuffer, unsigned nbytes, int compressionLevel,
					  void **outbuffer, unsigned *outbytes);
static void zUncompress(const void *inbuffer, unsigned nbytes,
					  void **outbuffer, unsigned *outbytes);
#else
#include <QByteArray>
#endif


#ifdef WIN32
#ifndef ftello
#define ftello ftell
#endif
#endif

/*-----------------------------------------------------------------------------
 WRITE/OUTPUT FUNCTIONS
 -----------------------------------------------------------------------------*/
/// returns NULL on error, creates a new fmv file for output on success and returns ptr to context
FM_Context * FM_Create(const char *filename)
{
	FILE *f = fopen(filename, "w+b");
	if (!f) return 0;
	FM_Context * c = new FM_Context;
	c->file = f;
	c->isOutput = true;
	c->imgOffsets.clear();
	c->imgOffsets.reserve(16);
	c->imgSizes.clear();
	c->imgSizes.reserve(16);
	
	FM_Header h;
	memset(&h, 0, sizeof(h));
	memcpy(h.magic, FM_MAGIC_STR, strlen(FM_MAGIC_STR));
	if ( fwrite(&h, sizeof(h), 1, c->file) != 1 ) {
		delete c;
		c = 0;
	}
	return c;
}
/// returns true on success
bool         FM_AddFrame(FM_Context *c, const void *pixels, 
						 unsigned width, unsigned height, 
						 unsigned depth, FM_Fmt fmt, 
						 bool comp, unsigned duration_ms)
{
	if (!c || !c->file || !c->isOutput || !width || !height || !depth) return false;
	//off_t savedOff = ftello(c->file);
	fseek(c->file, 0, SEEK_END);
	off_t imgoffset = ftello(c->file);
	if (c->imgOffsets.capacity() == c->imgOffsets.size()) {
		c->imgOffsets.reserve(c->imgOffsets.capacity()*2);
		c->imgSizes.reserve(c->imgOffsets.capacity());
	}
	c->imgOffsets.push_back(imgoffset);
	c->imgSizes.push_back(ImgSize(width, height));
	
	FM_ImageDescriptor desc;
	memset(&desc, 0, sizeof(desc));
	desc.magic = FM_IMAGE_DESCRIPTOR_MAGIC;
	desc.width = width;
	desc.height = height;
	desc.comp = comp ? 1 : 0;
	desc.duration = duration_ms;
	desc.fmt = fmt;
	
	int pixsz = 1;
	switch (depth) {
		case 32: ++pixsz; 
		case 24: ++pixsz; 
		case 16: ++pixsz; 
		case 8:
			break;
		default:
			//fseeko(c->file, savedOff, SEEK_SET);
			return false;
	}
	
#ifndef NO_QT
	QByteArray d;
#endif
	void *data = (void *)pixels;
	unsigned datasz = width*height*pixsz;
	if (comp) {
		data = 0;
#ifdef NO_QT
		zCompress(pixels, datasz, 9, &data, &datasz);
#else
		d = qCompress((uchar *)pixels, datasz, 9);
		data = (void *)d.constData();
		if (!data) {
			return false;
		}
		datasz = d.size();
#endif
		
	}
	bool ret = false;
	desc.length = datasz;
	if ( data 
		 && fwrite(&desc, sizeof(desc), 1, c->file) == 1 
		 && (!datasz || fwrite(data, datasz, 1, c->file) == 1) ) {
		// update header .. tell it how many images we have
		fseek(c->file, 0, SEEK_SET);
		FM_Header h;
		if ( fread(&h, sizeof(h), 1, c->file) == 1 ) {
			h.nFrames = c->imgOffsets.size();
			fseek(c->file, 0, SEEK_SET);
			fwrite(&h, sizeof(h), 1, c->file);
		}
		ret = true;
	}
	
#ifdef NO_QT
	if (comp) {
		free(data);
		data = 0;
	}
#endif
	
	//fseeko(c->file, savedOff, SEEK_SET);
	
	return ret;
}

/*-----------------------------------------------------------------------------
 READ/INPUT FUNCTIONS
 -----------------------------------------------------------------------------*/
/// open .fmv file for input. returns pointer to context on success
FM_Context * FM_Open(const char *filename)
{
	FILE *f = fopen(filename, "rb");
	if (!f) return 0;
	FM_Header h;
	memset(&h, 0, sizeof(h));
	if (fread(&h, sizeof(h), 1, f) != 1) { fclose(f); return 0; }
	if (strncmp(h.magic, FM_MAGIC_STR, sizeof(h.magic))) { fclose(f); return 0; }
	
	FM_Context *c = new FM_Context;
	c->isOutput = false;
	c->file = f;
	c->imgOffsets.reserve(h.nFrames);
	c->imgSizes.reserve(h.nFrames);
	// scan file..
	for (unsigned i = 0; i < h.nFrames; ++i) {
		c->imgOffsets.push_back(ftello(f)); // save image offset..
		FM_ImageDescriptor desc;
		memset(&desc, 0, sizeof(desc));
		if ( fread(&desc, sizeof(desc), 1, f) != 1 ) {
			delete c;
			return 0;
		}
		c->imgSizes.push_back(ImgSize(desc.width, desc.height));
		
		// now advance to next image
		if ( fseek(f, desc.length, SEEK_CUR) ) {
			// eek! seek error!  image file corrupt/truncated??
			delete c;
			return 0;
		}
	}
	return c;
}

/// returns true if the filename is openable and readable as an .fmv file!
bool         FM_IsFMV(const char *filename)
{
	FILE *f = fopen(filename, "rb");
	if (!f) return false;
	FM_Header h;
	memset(&h, 0, sizeof(h));
	bool ret = true;
	fread(&h, sizeof(h), 1, f);
	if (strncmp(h.magic, FM_MAGIC_STR, sizeof(h.magic))) { ret = false; }
	fclose(f);
	return ret;
}

/// read a frame from the .fmv file
FM_Image * FM_ReadFrame(FM_Context *c, unsigned frame_id /* first frame is frame 0 */)
{
	if (!c || c->isOutput || frame_id >= c->imgOffsets.size()) return 0;
	
//	off_t savedOff = ftello(c->file);
	if ( fseek(c->file, c->imgOffsets[frame_id], SEEK_SET) ) {
		return 0;
	}

	FM_Image *ret = new FM_Image;
	if ( fread(&ret->desc, sizeof(ret->desc), 1, c->file) != 1
		 || ret->desc.magic != FM_IMAGE_DESCRIPTOR_MAGIC) {
		delete ret; 
		ret = 0;
	} else {		
#ifdef NO_QT
		ret->data = (uint8_t *)malloc(ret->desc.length);
		if (!ret->data) {
			delete ret;
			ret = 0;
		} else {
			if ( fread((void *)ret->data, ret->desc.length, 1, c->file) != 1 ) {
				delete ret;
				ret = 0;
			} else if (ret->desc.comp) {
				void *out = 0;
				unsigned outbytes = 0;
				zUncompress(ret->data, ret->desc.length, &out, &outbytes);
				if (!out) {
					delete ret;
					ret = 0;
				} else {
					ret->desc.comp = 0;
					ret->desc.length = outbytes;
					free(ret->data);
					ret->data = (uint8_t *)out;
#else
		ret->data.resize(ret->desc.length);
		if (ret->data.size() != (int)ret->desc.length) {
			delete ret;
			ret = 0;
		} else {
			if ( fread((void *)ret->data.data(), ret->data.size(), 1, c->file) != 1 ) {
				delete ret;
				ret = 0;
			} else if (ret->desc.comp) {
				ret->data = qUncompress(ret->data);
				if (ret->data.isNull()) {
					delete ret;
					ret = 0;
				} else {
					ret->desc.comp = 0;
					ret->desc.length = ret->data.size();
#endif
		
				}
			}
		}
	}
	
//	fseek(c->file, savedOff, SEEK_SET);
	return ret;
}

/*-----------------------------------------------------------------------------
 READ/WRITE FUNCTIONS (applicable to both)
 -----------------------------------------------------------------------------*/
/** call this on input or output.  MANDATORY for output: to finalize the image
 in both cases frees the context pointer, closes file, etc. */
void         FM_Close(FM_Context *c)
{
	delete c; 
	// NB we dont need to update header for output mode since we did it in realtime as we added frames..
}


#if defined(NO_QT)
void zCompress(const void* data, unsigned nbytes, int compressionLevel,
			   void **out, unsigned *outbytes)
{
    if (nbytes == 0) {
		*out = malloc(4);
		memset(*out, 0, 4);
		*outbytes = 4;
        return ;
    }
    if (!data) {
        return;
	}
    if (compressionLevel < -1 || compressionLevel > 9)
        compressionLevel = -1;
	
    unsigned long len = nbytes + nbytes / 100 + 13;
    int res;
	*out = 0;
	*outbytes = 0;
	unsigned char *bazip;
    do {
		*out = realloc(*out, len + 4);
        res = ::compress2(((unsigned char*)(*out))+4, &len, (unsigned char*)data, nbytes, compressionLevel);
		
        switch (res) {
			case Z_OK:
				*out = realloc(*out, len + 4);
				bazip = (unsigned char *)*out;
				bazip[0] = (nbytes & 0xff000000) >> 24;
				bazip[1] = (nbytes & 0x00ff0000) >> 16;
				bazip[2] = (nbytes & 0x0000ff00) >> 8;
				bazip[3] = (nbytes & 0x000000ff);
				*outbytes = len + 4;
				break;
			case Z_MEM_ERROR:
				//qWarning("qCompress: Z_MEM_ERROR: Not enough memory");
				free(*out);
				*out = 0;
				break;
			case Z_BUF_ERROR:
				len *= 2;
				break;
        }
    } while (res == Z_BUF_ERROR);
}

void zUncompress(const void *d, unsigned nbytes,
				 void **out, unsigned *outbytes)
{
	const char *data = (const char *)d;
	if (!data || !out || !outbytes) {
//		qWarning("qUncompress: Data is null");
		return;
	}
	*out = 0;
	*outbytes = 0;
	
	if (nbytes <= 4) {
		//if (nbytes < 4 || (data[0]!=0 || data[1]!=0 || data[2]!=0 || data[3]!=0))
		//	qWarning("qUncompress: Input data is corrupted");
		return;
	}
	unsigned long expectedSize = (data[0] << 24) | (data[1] << 16) | (data[2] <<  8) | (data[3]      );
	unsigned long len = expectedSize;
	if (!len) len = 1;
	
	unsigned char *p = 0;
	
	while(1) {
		unsigned long alloc = len;
		if (len  >= static_cast<unsigned long>(1 << 31)) {
			//qWarning("qUncompress: Input data is corrupted");
			return;
		}
		p = (unsigned char *)realloc(p, alloc);
		if (!p) {
			return;
		}
		
		int res = ::uncompress((unsigned char*)p, &len, ((unsigned char*)data)+4, nbytes-4);
		
		switch (res) {
			case Z_OK:
				if (len != alloc) {
					if (len  >= (unsigned long)(1 << 31)) {
						//QByteArray does not support that huge size anyway.
						//qWarning("qUncompress: Input data is corrupted");
						free(p);
						return;
					}
				}
				*out = p;
				*outbytes = len;
				return;
				
				free(p);
				return;
				
			case Z_BUF_ERROR:
				len *= 2;
				continue;
				
			case Z_MEM_ERROR:
			case Z_DATA_ERROR:
			default:
				free(p);
				return;
		}
	}		
}
#endif
