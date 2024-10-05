/*
 * Graphics routine for PalmOS5 ARM native.
 *   Copyright(c)2003 ZOBplus hiro <hiro@zob.ne.jp>
 */

#include <PalmOS.h>

typedef unsigned char IndexedColorType;

unsigned char *_baseP;
unsigned int   _rowBytes;

void
ARMGChromakeyPaintEntry(const void *emulStateP_,
			void *userData68KP_,
			Call68KFuncType *call68KFuncP)
{
  
}

static inline unsigned int
GCoord2Offset(Coord x_, Coord y_)
{
  return y_ * _rowBytes + x_;
}

static inline void
GSetPixel(Coord x_, Coord y_, IndexedColorType c_)
{
  unsigned int o  = GCoord2Offset(x_, y_);
  _baseP[o] = c_;
}

static inline IndexedColorType
GFetchPixel(Coord x_, Coord y_)
{
  unsigned int o = GCoord2Offset(x_, y_);
  return _baseP[o];
}

void
GChromakeyPaint(RectanglePtr clipP_, UInt8 *patP_, Boolean line_)
{
  IndexedColorType c;
  WinHandle        cur;
  UInt8            r, g, b, off;
  Coord            x, y, dx, lx, ox;
  UInt16           i, j, n, coordSys;
  UInt16           bl, bu, br, bd;
  RectangleType    rs;
  UInt8            pattern[][3] = {
    { 0x00, 0x00, 0x00 },
    { 0xff, 0x00, 0x00 },
    { 0x00, 0xff, 0x00 },
    { 0xff, 0xff, 0x00 },
    { 0x00, 0x00, 0xff },
    { 0xff, 0x00, 0xff },
    { 0x00, 0xff, 0xff },
    { 0xff, 0xff, 0xff },
  };
  i = 0;
  n = patP_[i++];
  for (j = 0 ; j < n ; j++) {
    pattern[j + 1][0] = patP_[i + j * 3 + 0];
    pattern[j + 1][1] = patP_[i + j * 3 + 1];
    pattern[j + 1][2] = patP_[i + j * 3 + 2];
  }

  bl = clipP_->topLeft.x;
  bu = clipP_->topLeft.y;
  br = bl + clipP_->extent.x;
  bd = bu + clipP_->extent.y;
  cur = WinGetDrawWindow();
  coordSys = WinSetCoordinateSystem(kCoordinatesDouble);
  for (y = bu; y < bd ; y++) {
    for (x = bl ; x < br ; x += 8) {
      lx = (br - x < 8) ? br - x : 8;
      for (dx = 0, ox = 7 ; dx < lx ; dx++, ox--) {
	c   = GFetchPixel(x + dx, y);
	off = 1 << ox;
	for (j = 1 ; j <= n ; j++) {
	  if (_colors[j] == c) {
	    b = (pattern[j][0] & off) >> ox;
	    r = (pattern[j][1] & off) >> ox;
	    g = (pattern[j][2] & off) >> ox;
	    GSetPixel(x + dx, y, _colors[(r << 1) | (g << 2) | b]);
	    break;
	  }
	}
      }
    }
    if (line_) {
      RctSetRectangle(&rs, bl, y, br - bl, 1);
      WinCopyRectangle(_canvas, cur, &rs, bl, y, winPaint);
    }
  }
  if (!line_) {
    WinCopyRectangle(_canvas, cur, clipP_, bl, bu, winPaint);
  }
  WinSetCoordinateSystem(coordSys);
}
