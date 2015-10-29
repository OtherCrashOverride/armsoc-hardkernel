/* -*- mode: C; c-file-style: "k&r"; tab-width 4; indent-tabs-mode: t; -*- */

/*
 * Copyright © 2011 Texas Instruments, Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Rob Clark <rob@ti.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "armsoc_driver.h"
#include "armsoc_exa.h"

#include "exa.h"


//*********************************************************************
// Note: The Exynos G2D changes should be moved to thier own EXA driver
//       so that armsoc_exa_null continues to work for other hardware
//*********************************************************************

// G2D
//#include <sys/stat.h>
//#include <fcntl.h>
#include "libdrm.h"
#include "exynos_drm.h"
#include "fimg2d_reg.h"
#include "fimg2d.h"


// for some reason these are defined in armsoc_dumb.c instead
// of in a header file
struct armsoc_device {
	int fd;
	int (*create_custom_gem)(int fd, struct armsoc_create_gem *create_gem);
	Bool alpha_supported;
};

struct armsoc_bo {
	struct armsoc_device *dev;
	uint32_t handle;
	uint32_t size;
	void *map_addr;
	uint32_t fb_id;
	uint32_t width;
	uint32_t height;
	uint8_t depth;
	uint8_t bpp;
	uint32_t pitch;
	int refcnt;
	int dmabuf;
	/* initial size of backing memory. Used on resize to
	* check if the new size will fit
	*/
	uint32_t original_size;
	uint32_t name;
};


/* For easy reference only
struct g2d_image {
	enum e_g2d_select_mode		select_mode;
	enum e_g2d_color_mode		color_mode;
	enum e_g2d_repeat_mode		repeat_mode;
	enum e_g2d_scale_mode		scale_mode;
	unsigned int			xscale;
	unsigned int			yscale;
	unsigned char			rotate_90;
	unsigned char			x_dir;
	unsigned char			y_dir;
	unsigned char			component_alpha;
	unsigned int			width;
	unsigned int			height;
	unsigned int			stride;
	unsigned int			need_free;
	unsigned int			color;
	enum e_g2d_buf_type		buf_type;
	unsigned int			bo[G2D_PLANE_MAX_NR];
	struct drm_exynos_g2d_userptr	user_ptr[G2D_PLANE_MAX_NR];
	void				*mapped_ptr[G2D_PLANE_MAX_NR];
};
*/


/*
static void Dump_armsoc_bo(struct armsoc_bo* bo)
{
	xf86DrvMsg(-1, X_ERROR, "Dump_armsoc_bo: map_addr=%x, size=%d width=%d height=%d depth=%d bpp=%d pitch=%d\n", (unsigned int)bo->map_addr, bo->size, bo->width, bo->height, bo->depth, bo->bpp, bo->pitch);
}
*/


/* This file has a trivial EXA implementation which accelerates nothing.  It
 * is used as the fall-back in case the EXA implementation for the current
 * chipset is not available.  (For example, on chipsets which used the closed
 * source IMG PowerVR EXA implementation, if the closed-source submodule is
 * not installed.
 */

struct ARMSOCNullEXARec {
	struct ARMSOCEXARec base;
	ExaDriverPtr exa;
	/* add any other driver private data here.. */
	struct g2d_context* ctx;
	PixmapPtr pSource;
	int xdir;
	int ydir;
};

static Bool
PrepareSolidFail(PixmapPtr pPixmap, int alu, Pixel planemask, Pixel fill_colour)
{
	return FALSE;
}

static Bool
PrepareCopy(PixmapPtr pSrc, PixmapPtr pDst, int xdir, int ydir,
	int alu, Pixel planemask)
{
	ScrnInfoPtr pScrn = xf86ScreenToScrn(pDst->drawable.pScreen);
	struct ARMSOCRec* pARMSOC = ARMSOCPTR(pScrn);
	struct ARMSOCNullEXARec* nullExaRec = (struct ARMSOCNullEXARec*)pARMSOC->pARMSOCEXA;

	struct ARMSOCPixmapPrivRec* srcPriv = exaGetPixmapDriverPrivate(pSrc);
	struct ARMSOCPixmapPrivRec* dstPriv = exaGetPixmapDriverPrivate(pDst);


	/*
	xf86DrvMsg(-1, X_ERROR, "PrepareCopy; pSrc=%x, pDst=%x, xdir=%d, ydir=%d, alu=%x, planemask=%x\n",
		(unsigned int)pSrc, (unsigned int)pDst,
		xdir, ydir,
		(unsigned int)alu, (unsigned int)planemask);
	*/


	//Dump_armsoc_bo(srcPriv->bo);
	//Dump_armsoc_bo(dstPriv->bo);

	//if (pSrc->drawable.width < 32 || pSrc->drawable.height < 32 ||
	//	pDst->drawable.width < 32 || pDst->drawable.height < 32)
	//{
	//	return FALSE;
	//}


	// If there are no buffer objects, fallback
	if (!srcPriv->bo || !dstPriv->bo)
	{
		return FALSE;
	}

	//// If the image is too small, fallback
	//// Note: This is not the size of the copy, rather its the size of the buffer.
	////       It is here to avoid performing icon blits during testing.
	////       This lessons the chance of a kernel page fault until the 
	////       kernel DRM driver is fixed
	//if ((srcPriv->bo->width < 256 || srcPriv->bo->height < 256) ||
	//	(dstPriv->bo->width < 256 || dstPriv->bo->height < 256))
	//{
	//	return FALSE;
	//}

	//// If bo is not 16 or 32bit, fallback
	//if ((srcPriv->bo->bpp != 16 && srcPriv->bo->bpp != 32) ||
	//	(dstPriv->bo->bpp != 16 && dstPriv->bo->bpp != 32))
	//{
	//	return FALSE;
	//}

	// If bo is not 32bit, fallback
	if ((srcPriv->bo->bpp != 32) ||
		(dstPriv->bo->bpp != 32))
	{
		return FALSE;
	}

	//if (armsoc_bo_map(srcPriv->bo) == 0 ||
	//	armsoc_bo_map(dstPriv->bo) == 0)
	//{
	//	return FALSE;
	//}


	//// Debug
	//if (xdir < 1 || ydir < 1)
	//{
	//	return FALSE;
	//}


	//armsoc_bo_cpu_prep(srcPriv->bo, ARMSOC_GEM_READ);
	//armsoc_bo_cpu_prep(dstPriv->bo, ARMSOC_GEM_READ_WRITE);
	

	if (ARMSOCPrepareAccess(pSrc, EXA_PREPARE_SRC) == FALSE)
	{
		return FALSE;
	}
	
	if (ARMSOCPrepareAccess(pDst, EXA_PREPARE_DEST) == FALSE)
	{
		ARMSOCFinishAccess(pSrc, EXA_PREPARE_SRC);
		return FALSE;
	}

	//ARMSOCRegisterExternalAccess(pSrc);
	//ARMSOCRegisterExternalAccess(pDst);

	// Save for later
	nullExaRec->pSource = pSrc;
	nullExaRec->xdir = xdir;
	nullExaRec->ydir = ydir;



	return TRUE;
}

/**
* Copy - perform a copy between two pixmaps
* @pDstPixmap: destination Pixmap
* @srcX: source X coordinate
* @srcY: source Y coordinate
* @dstX: destination X coordinate
* @dstY: destination Y coordinate
* @width: copy width
* @height: copy height
*
* Perform the copy setup by the previous PrepareCopy call, from
* (@srcX,@srcY) in the source pixmap to (@dstX,@dstY) in pDstPixmap using
* @width and @height to determine the quantity of the copy.
*
* Must not fail.
*
* Required.
*/
static void ExaCopy(PixmapPtr pDstPixmap, int srcX, int srcY, int dstX, int dstY,
	    int width, int height)
{
	ScrnInfoPtr pScrn = xf86ScreenToScrn(pDstPixmap->drawable.pScreen);
	struct ARMSOCRec* pARMSOC = ARMSOCPTR(pScrn);
	struct ARMSOCNullEXARec* nullExaRec = (struct ARMSOCNullEXARec*)pARMSOC->pARMSOCEXA;

	struct ARMSOCPixmapPrivRec* srcPriv = exaGetPixmapDriverPrivate(nullExaRec->pSource);
	struct ARMSOCPixmapPrivRec* dstPriv = exaGetPixmapDriverPrivate(pDstPixmap);

	struct g2d_image srcImage;
	struct g2d_image dstImage;

	//int i;
	//unsigned char* ptr;
	//int dummy;

	/*
	xf86DrvMsg(-1, X_ERROR, "ExaCopy :srcX=%d, srcY=%d, dstX=%d, dstY=%d, width=%d, height=%d | src_bpp=%d, dst_bpp=%d\n",
		srcX, srcY, dstX, dstY, width, height, srcPriv->bo->bpp, dstPriv->bo->bpp);
	*/
	
	//dstPriv->bo->refcnt += 1;


	//--------------

	// Set the source blit format
	switch (srcPriv->bo->bpp)
	{
	case 32:
		srcImage.color_mode = G2D_COLOR_FMT_ARGB8888 | G2D_ORDER_AXRGB;
		break;

	case 16:
		srcImage.color_mode = G2D_COLOR_FMT_RGB565;
		break;

	default:
		// Not supported			
		break;
	}

	srcImage.select_mode = G2D_SELECT_MODE_NORMAL;
	srcImage.repeat_mode = G2D_REPEAT_MODE_NONE;
	srcImage.scale_mode = G2D_SCALE_MODE_NONE;
	srcImage.xscale = 1;
	srcImage.yscale = 1;
	srcImage.rotate_90 = 0;
	srcImage.x_reverse_flag = (nullExaRec->xdir < 1);
	srcImage.y_reverse_flag = (nullExaRec->ydir < 1);
	srcImage.component_alpha = 0;
	srcImage.width = srcPriv->bo->width;
	srcImage.height = srcPriv->bo->height;
	srcImage.stride = srcPriv->bo->pitch; 
	srcImage.need_free = 0;
	srcImage.color = 0;

	//if (srcPriv->bo->map_addr == 0)
	//{
	//	srcImage.buf_type = G2D_IMGBUF_GEM;
	//	srcImage.bo[0] = srcPriv->bo->handle;
	//}
	//else
	//{
		srcImage.buf_type = G2D_IMGBUF_USERPTR;
		srcImage.user_ptr[0].userptr = (unsigned long)srcPriv->bo->map_addr;
		srcImage.user_ptr[0].size = srcPriv->bo->size;
	//}

	//srcPriv->bo->refcnt += 1;


	//--------------

	// Set the destination blit format
	switch (dstPriv->bo->bpp)
	{
	case 32:
		dstImage.color_mode = G2D_COLOR_FMT_ARGB8888 | G2D_ORDER_AXRGB;
		break;

	case 16:
		dstImage.color_mode = G2D_COLOR_FMT_RGB565;
		break;

	default:
		// Not supported
		break;
	}

	dstImage.select_mode = G2D_SELECT_MODE_NORMAL;

	dstImage.repeat_mode = G2D_REPEAT_MODE_NONE;
	dstImage.scale_mode = G2D_SCALE_MODE_NONE;
	dstImage.xscale = 1;
	dstImage.yscale = 1;
	dstImage.rotate_90 = 0;
	dstImage.x_reverse_flag = srcImage.x_reverse_flag;
	dstImage.y_reverse_flag = srcImage.y_reverse_flag;
	dstImage.component_alpha = 0;
	dstImage.width = dstPriv->bo->width;
	dstImage.height = dstPriv->bo->height;
	dstImage.stride = dstPriv->bo->pitch;
	dstImage.need_free = 0;
	dstImage.color = 0xffffffff;
	
	//if (dstPriv->bo->map_addr == 0)
	//{
	//	dstImage.buf_type = G2D_IMGBUF_GEM;
	//	dstImage.bo[0] = dstPriv->bo->handle;
	//}
	//else
	//{
		dstImage.buf_type = G2D_IMGBUF_USERPTR;
		dstImage.user_ptr[0].userptr = (unsigned long)dstPriv->bo->map_addr;
		dstImage.user_ptr[0].size = dstPriv->bo->size;
	//}

	//// Debug: Touch all memory to bring it in
	//ptr = (unsigned char*)srcPriv->bo->map_addr;
	//for (i = 0; i < srcPriv->bo->size; i += 4096)
	//{
	//	dummy += ptr[i];
	//}

	//ptr = (unsigned char*)dstPriv->bo->map_addr;
	//for (i = 0; i < dstPriv->bo->size; i += 4096)
	//{
	//	dummy += ptr[i];
	//}

	// Debug testing - solid fill source
	//g2d_solid_fill(ctx, &srcImage, srcX, srcY, width, height);


	/*
	g2d_copy(struct g2d_context *ctx, struct g2d_image *src,
		struct g2d_image *dst, unsigned int src_x, unsigned int src_y,
		unsigned int dst_x, unsigned dst_y, unsigned int w,
		unsigned int h)
		*/
	g2d_copy(nullExaRec->ctx, &srcImage, &dstImage, srcX, srcY, dstX, dstY, width, height);


	// Debug testing - solid fill destination
	/*
	g2d_solid_fill(struct g2d_context *ctx, struct g2d_image *img,
			unsigned int x, unsigned int y, unsigned int w,
			unsigned int h)
	*/
	//g2d_solid_fill(ctx, &dstImage, dstX, dstY, width, height);

	
	// Perform the G2D operations
	g2d_exec(nullExaRec->ctx);

	//dstPriv->bo->refcnt -= 1;
}


/**
* DoneCopy - finish a copy operation
* @pDstPixmap: Pixmap to complete
*
* Tear down the copy operation for @pDstPixmap, if necessary.
*
* Must not fail.
*
* Required.
*/
static void ExaDoneCopy(PixmapPtr pDstPixmap)
{
	ScrnInfoPtr pScrn = xf86ScreenToScrn(pDstPixmap->drawable.pScreen);
	struct ARMSOCRec* pARMSOC = ARMSOCPTR(pScrn);
	struct ARMSOCNullEXARec* nullExaRec = (struct ARMSOCNullEXARec*)pARMSOC->pARMSOCEXA;


	//g2d_exec(ctx);

	
	//armsoc_bo_cpu_fini(srcPriv->bo, ARMSOC_GEM_READ);
	//armsoc_bo_cpu_fini(dstPriv->bo, ARMSOC_GEM_READ_WRITE);
	
	ARMSOCFinishAccess(nullExaRec->pSource, EXA_PREPARE_SRC);
	ARMSOCFinishAccess(pDstPixmap, EXA_PREPARE_DEST);

	//ARMSOCDeregisterExternalAccess(nullExaRec->pSource);
	//ARMSOCDeregisterExternalAccess(pDstPixmap);

	//srcPriv->bo->refcnt -= 1;
}


static Bool
CheckCompositeFail(int op, PicturePtr pSrcPicture, PicturePtr pMaskPicture,
		PicturePtr pDstPicture)
{
	return FALSE;
}

static Bool
PrepareCompositeFail(int op, PicturePtr pSrcPicture, PicturePtr pMaskPicture,
		PicturePtr pDstPicture, PixmapPtr pSrc,
		PixmapPtr pMask, PixmapPtr pDst)
{
	return FALSE;
}

/**
 * CloseScreen() is called at the end of each server generation and
 * cleans up everything initialised in InitNullEXA()
 */
static Bool
CloseScreen(CLOSE_SCREEN_ARGS_DECL)
{
	ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
	struct ARMSOCRec *pARMSOC = ARMSOCPTR(pScrn);

	struct ARMSOCNullEXARec* nullExaRec = (struct ARMSOCNullEXARec*)pARMSOC->pARMSOCEXA;

	// Clean up G2D
	g2d_fini(nullExaRec->ctx);


	exaDriverFini(pScreen);
	free(((struct ARMSOCNullEXARec *)pARMSOC->pARMSOCEXA)->exa);
	free(pARMSOC->pARMSOCEXA);
	pARMSOC->pARMSOCEXA = NULL;

	return TRUE;
}

/* FreeScreen() is called on an error during PreInit and
 * should clean up anything initialised before InitNullEXA()
 * (which currently is nothing)
 *
 */
static void
FreeScreen(FREE_SCREEN_ARGS_DECL)
{
}

struct ARMSOCEXARec *
InitNullEXA(ScreenPtr pScreen, ScrnInfoPtr pScrn, int fd)
{
	struct ARMSOCNullEXARec *null_exa;
	struct ARMSOCEXARec *armsoc_exa;
	ExaDriverPtr exa;

	INFO_MSG("Exynos G2D EXA mode");

	null_exa = calloc(1, sizeof(*null_exa));
	if (!null_exa)
		goto out;

	armsoc_exa = (struct ARMSOCEXARec *)null_exa;

	exa = exaDriverAlloc();
	if (!exa)
		goto free_null_exa;

	null_exa->exa = exa;

	exa->exa_major = EXA_VERSION_MAJOR;
	exa->exa_minor = EXA_VERSION_MINOR;

	exa->pixmapOffsetAlign = 0;
	exa->pixmapPitchAlign = 32;
	exa->flags = EXA_OFFSCREEN_PIXMAPS |
			EXA_HANDLES_PIXMAPS | EXA_SUPPORTS_PREPARE_AUX;
	exa->maxX = 4096;
	exa->maxY = 4096;

	/* Required EXA functions: */
	exa->WaitMarker = ARMSOCWaitMarker;
	exa->CreatePixmap2 = ARMSOCCreatePixmap2;
	exa->DestroyPixmap = ARMSOCDestroyPixmap;
	exa->ModifyPixmapHeader = ARMSOCModifyPixmapHeader;

	exa->PrepareAccess = ARMSOCPrepareAccess;
	exa->FinishAccess = ARMSOCFinishAccess;
	exa->PixmapIsOffscreen = ARMSOCPixmapIsOffscreen;

	/* Always fallback for software operations */
	exa->PrepareCopy = PrepareCopy;
	exa->PrepareSolid = PrepareSolidFail;
	exa->CheckComposite = CheckCompositeFail;
	exa->PrepareComposite = PrepareCompositeFail;
	exa->Copy = ExaCopy;
	exa->DoneCopy = ExaDoneCopy;

	if (!exaDriverInit(pScreen, exa)) {
		ERROR_MSG("exaDriverInit failed");
		goto free_exa;
	}

	armsoc_exa->CloseScreen = CloseScreen;
	armsoc_exa->FreeScreen = FreeScreen;


	// Initialize a G2D context
	xf86DrvMsg(-1, X_ERROR, "G2D: fd = %d\n", fd);

	null_exa->ctx = g2d_init(fd);
	xf86DrvMsg(-1, X_ERROR, "G2D: ctx = %u\n", (unsigned int)null_exa->ctx);


	return armsoc_exa;

free_exa:
	free(exa);
free_null_exa:
	free(null_exa);
out:
	return NULL;
}

