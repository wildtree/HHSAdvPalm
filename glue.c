/*
 * Compalibility Library for PalmOS 3.5/4/5
 */

#ifndef ALLOW_ACCESS_TO_INTERNALS_OF_BITMAPS
#define ALLOW_ACCESS_TO_INTERNALS_OF_BITMAPS
#endif /* ALLOW_ACCESS_TO_INTERNALS_OF_BITMAPS */

#include <PalmOS.h>
#include "glue.h"

Boolean
GlueBmpIsCompressed(BitmapPtr bmpP)
{
  if (SysGetTrapAddress(HDSelectorBmpGetCompressionType) !=
      SysGetTrapAddress(sysTrapSysUnimplemented)) {
    return BmpGetCompressionType(bmpP) != BitmapCompressionTypeNone;
  }
  return bmpP->flags.compressed;
}

void
GlueBmpGetDimensions(BitmapPtr bmpP,
		     Coord *widthP, Coord *heightP, UInt16 *rowBytesP)
{
  if (SysGetTrapAddress(sysTrapBmpGetDimensions) !=
      SysGetTrapAddress(sysTrapSysUnimplemented)) {
    BmpGetDimensions(bmpP, widthP, heightP, rowBytesP);
    return;
  }
  if (bmpP) {
    if (widthP) {
      *widthP = bmpP->width;
    }
    if (heightP) {
      *heightP = bmpP->height;
    }
    if (rowBytesP) {
      *rowBytesP = bmpP->rowBytes;
    }
  }
}

void
GlueBmpGetSizes(BitmapPtr bmpP, UInt32 *dataSizeP, UInt32 *headerSizeP)
{
  if (SysGetTrapAddress(sysTrapBmpGetSizes) !=
      SysGetTrapAddress(sysTrapSysUnimplemented)) {
    BmpGetSizes(bmpP, dataSizeP, headerSizeP);
    return;
  }
  if (bmpP) {
    if (dataSizeP) {
      *dataSizeP = BmpSize(bmpP) - sizeof(BitmapType);
    }
    if (headerSizeP) {
      *headerSizeP = sizeof(BitmapType);
    }
  }
}
