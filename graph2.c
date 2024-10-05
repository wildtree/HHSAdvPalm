/*
 * Graphics routine for High-High-School Adventure.
 *  Copyright(c)2003 ZOBplus hiro
 */

#include <PalmOS.h>
#include <SonyCLIE.h>
#include "graph2.h"
#include "glue.h"

#define MAX_QUEUE_SIZE (1024)
typedef struct {
  int       _Qstart, _Qend;
  PointType _Queue[MAX_QUEUE_SIZE];
} GPointQueueType;
typedef GPointQueueType *GPointQueuePtr;

/* global variables */

static GPointQueueType  _pointQ;
static Boolean          _hDensity; /* OS5 high-density */
static UInt16           _hrLib;    /* OS4 CLIe High-Resolution */
static UInt16           _hrVer;    /* CLIe High-Resolution API Version */
static IndexedColorType _colors[gColorNameCandidate]; /* color map */
static RGBColorType     _defrgb[gColorNameCandidate]; /* dafault map */
static WinHandle        _canvas;   /* offscreen window */
static BitmapPtr        _bmpP;     /* canvas bitmap */
static UInt8            *_baseP;   /* canvas bitmap body */
static UInt16           _rowBytes; /* row bytes */
static RectangleType    _border;   /* canvas border */
static UInt32           _width, _height, _depth; /* properties */
static FontID           _fntTable[8]; /* for CLIe High Resolution Fonts */

static void
HDWinDrawChars(Char *strP_, UInt16 len_, Coord x_, Coord y_)
{
  WinHandle     curH, offH;
  BitmapPtr     bmpP;
  BitmapPtrV3   bmp3P;
  RectangleType r;
  Coord         w, h;
  Err           err;
  UInt16        coordSys;

  curH = WinGetDrawWindow();
  coordSys = WinSetCoordinateSystem(kCoordinatesDouble);
  w = FntCharsWidth(strP_, len_);
  h = FntCharHeight();
  bmpP  = BmpCreate(w, h, 8, NULL, &err);
  bmp3P = BmpCreateBitmapV3(bmpP, kCoordinatesStandard,
                            BmpGetBits(bmpP), NULL);
  offH  = WinCreateBitmapWindow((BitmapPtr)bmp3P, &err);

  WinSetDrawWindow(offH);
  WinDrawChars(strP_, len_, 0, 0);
  WinDeleteWindow(offH, false);
  BmpSetDensity((BitmapPtr)bmp3P, kCoordinatesDouble);
  offH  = WinCreateBitmapWindow((BitmapPtr)bmp3P, &err);
  RctSetRectangle(&r, 0, 0, w / 2, h / 2);
  WinCopyRectangle(offH, curH, &r, x_, y_, winOverlay);
  WinDeleteWindow(offH, false);
  BmpDelete((BitmapPtr)bmp3P);
  BmpDelete(bmpP);
  WinSetCoordinateSystem(coordSys);
}

static inline UInt16
GCoord2Offset(Coord x_, Coord y_)
{
  if (_hDensity || _hrLib) { /* High Resolution */
    return y_ * _rowBytes + x_;
  } else {
    return y_ * _rowBytes + (x_ / 2);
  }
}

static inline void
GSetPixel(Coord x_, Coord y_, IndexedColorType c_)
{
  UInt16 o  = GCoord2Offset(x_, y_);
  if (_hDensity) {
#if 0
    WinHandle draw = WinGetDrawWindow();
    WinSetDrawWindow(_canvas);
    WinDrawPixel(x_, y_);
    WinSetDrawWindow(draw);
#else 
    _baseP[o] = c_;
#endif
  } else if (_hrLib) {
    _baseP[o] = c_;
  } else {
    if (x_ % 2) {
      _baseP[o] &= 0xf0;
      _baseP[o] |= (c_ & 0x0f);
    } else {
      _baseP[o] &= 0x0f;
      _baseP[o] |= (c_ << 4);
    }
  }
}

static inline IndexedColorType
GFetchPixel(Coord x_, Coord y_)
{
  UInt16 o = GCoord2Offset(x_, y_);
  if (_hDensity) {
#if 0
    IndexedColorType c;
    WinHandle        draw = WinGetDrawWindow();
    WinSetDrawWindow(_canvas);
    c = WinGetPixel(x_, y_);
    WinSetDrawWindow(draw);
    return c;
#else
    return _baseP[o];
#endif
  } else if (_hrLib) {
    return _baseP[o];
  } else {
    if (x_ % 2) {
      return _baseP[o] & 0x0f;
    } else {
      return (_baseP[o] & 0xf0) >> 4;
    }
  }
}

#define isSjis(k_) (((k_) >= 0x80 && (k_) < 0xa0)||((k_) >= 0xe0))

static inline void
GInitQueue(void)
{
  _pointQ._Qstart = _pointQ._Qend = 0;
}

static inline void 
GShiftPoint(Coord x_, Coord y_)
{
  _pointQ._Queue[_pointQ._Qstart].x = x_;
  _pointQ._Queue[_pointQ._Qstart].y = y_;
  if (++_pointQ._Qstart == MAX_QUEUE_SIZE) {
    _pointQ._Qstart = 0;
  }
}

static inline PointType*
GUnshiftPoint(void)
{
  PointType *p;
  if (_pointQ._Qstart == _pointQ._Qend) return NULL;
  p = &_pointQ._Queue[_pointQ._Qend++];
  if (_pointQ._Qend == MAX_QUEUE_SIZE) {
    _pointQ._Qend = 0;
  }
  return p;
}

static void
GInitColors(void)
{
  UInt16 i;
  RGBColorType rgb[] = {
    { 0, 0x00, 0x00, 0x00},
    { 0, 0x00, 0x00, 0xff},
    { 0, 0xff, 0x00, 0x00},
    { 0, 0xff, 0x00, 0xff},
    { 0, 0x00, 0xff, 0x00},
    { 0, 0x00, 0xff, 0xff},
    { 0, 0xff, 0xff, 0x00},
    { 0, 0xff, 0xff, 0xff},
    { 0, 0x55, 0x55, 0x55},
    { 0, 0x00, 0x00, 0x80},
    { 0, 0x80, 0x00, 0x00},
    { 0, 0x80, 0x00, 0x80},
    { 0, 0x00, 0x80, 0x00},
    { 0, 0x00, 0x80, 0x80},
    { 0, 0x80, 0x80, 0x00},
    { 0, 0xaa, 0xaa, 0xaa},
  };
  for (i = 0 ; i < gColorNameCandidate ; i++) {
    _colors[i] = WinRGBToIndex(&rgb[i]);
    WinIndexToRGB(_colors[i], &_defrgb[i]);
    if (_hrLib == 0 && !_hDensity) {
      _colors[i] = i;
    }
  }
}

/* API */

Boolean
GInit(void)
{
  SonySysFtrSysInfoP sonySysFtrSysInfoP; /* Sony CLIe System Information */
  UInt32             wmVer, wmAttr;
  Err                err = 0;
  Boolean            res = false;
  Boolean            col = true;
  UInt16             i;
  FontID             ftab[] = {
    hrTinyFont, hrTinyBoldFont, hrSmallFont, hrSmallBoldFont,
    hrStdFont, hrBoldFont, hrLargeFont, hrLargeBoldFont
  };
  /* OS5 High Density Check */
  _hDensity = false;
  if (FtrGet(sysFtrCreator, sysFtrNumWinVersion, &wmVer) == errNone) {
    if (wmVer >= 4) { /* hd interface existing */
      WinScreenGetAttribute(winScreenDensity, &wmAttr);
      if (wmAttr == kDensityDouble) {
        _hDensity = true;
	WinSetCoordinateSystem(kCoordinatesStandard);
      }
    }
  }
  /* CLIe High Resolution Check */
  if ((err = FtrGet(sonySysFtrCreator,
		    sonySysFtrNumSysInfoP,
		    (UInt32*)&sonySysFtrSysInfoP)) == 0) {
    if (sonySysFtrSysInfoP->libr & sonySysFtrSysInfoLibrHR) {
      /* High-resolution library available */
      if ((err = SysLibFind(sonySysLibNameHR, &_hrLib))) {
        if (err == sysErrLibNotFound) {
          err = SysLibLoad('libr', sonySysFileCHRLib, &_hrLib);
        }
      }
    }
  }
  if (!_hDensity && (err || _hrLib == 0)) {
    _hrLib = 0; /* no library existing */
    _width = _height = 160;
    _depth = 8;
    WinScreenMode(winScreenModeSet, &_width, &_height, &_depth, &col);
    res = true;
  } else {
    _width = _height = 320;
    _depth = 8;
    res = true; /* only High-resolution CLIE/OS5 high density device support */
    if (_hDensity) {
      WinScreenMode(winScreenModeSet, &_width, &_height, &_depth, &col);
    } else {
      HROpen(_hrLib);
      HRGetAPIVersion(_hrLib, &_hrVer);
      HRWinScreenMode(_hrLib,
		      winScreenModeSet, &_width, &_height, &_depth, &col);
      for (i = 0 ; i < 8 ; i++) {
	_fntTable[i] = ftab[i];
      }
    }
  }
  WinScreenMode(winScreenModeGet, NULL, NULL, NULL, &col);
  if (!col) {
    if (_hrLib && !_hDensity) {
      HRClose(_hrLib);
    }
    return false; /* mono-chrome mode */
  }
  GInitColors();
  RctSetRectangle(&_border, 0, 0, _width, _height);
  if (_hDensity) {
    UInt16 coordSys = WinSetCoordinateSystem(kCoordinatesDouble);
    _canvas = WinCreateOffscreenWindow(_width, _height,
				       nativeFormat, &err);
    WinSetCoordinateSystem(coordSys);
  } else if (_hrLib) {
    _canvas = HRWinCreateOffscreenWindow(_hrLib,
					 _width, _height,
					 screenFormat, &err);
  }
  if (_canvas == NULL) {
    res = false;
  } else {
    _bmpP  = WinGetBitmap(_canvas);
    _baseP = BmpGetBits(_bmpP);
    GlueBmpGetDimensions(_bmpP, NULL, NULL, &_rowBytes);
  }
  return res;
}

Boolean
GFinalize(void)
{
  if (_hrLib && !_hDensity) {
    HRClose(_hrLib);
  }
  if (_canvas) {
    WinDeleteWindow(_canvas, false);
  }
  return true;
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
  if (_hDensity) {
    coordSys = WinSetCoordinateSystem(kCoordinatesDouble);
  }
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
      if (_hrLib) {
	HRWinCopyRectangle(_hrLib, _canvas, cur, &rs, bl, y, winPaint);
      } else {
	WinCopyRectangle(_canvas, cur, &rs, bl, y, winPaint);
      }
    }
  }
  if (!line_) {
    if (_hrLib) {
      HRWinCopyRectangle(_hrLib, _canvas, cur, clipP_, bl, bu, winPaint);
    } else {
      WinCopyRectangle(_canvas, cur, clipP_, bl, bu, winPaint);
    }
  }
  if (_hDensity) {
    WinSetCoordinateSystem(coordSys);
  }
}

static inline Boolean
_chk(Coord x_, Coord y_, IndexedColorType c_, IndexedColorType b_)
{
  IndexedColorType c = GFetchPixel(x_, y_);
  return c != c_ && c != b_;
}

void
GPaintSpec(GPaintSpecPtr specP_)
{
  PointType        *pP;
  Coord            bl, bu, br, bd;
  Coord            l, r, x, y;
  Coord            wx, wy;
  WinHandle        cur;
  Coord            ltx, lty, lbx, lby;
  Boolean          uf, df;
  UInt16           coordSys;

  bl  = specP_->_clipP->topLeft.x;
  bu  = specP_->_clipP->topLeft.y;
  br  = bl + specP_->_clipP->extent.x;
  bd  = bu + specP_->_clipP->extent.y;
  x = specP_->_x + bl;
  y = specP_->_y + bu;

  ltx = lty = lbx = lby = -1;

  cur = WinGetDrawWindow();
  if (!_chk(x, y, _colors[specP_->_fg], _colors[specP_->_bg])){
    /* needless to paint */
    return;
  }
  GInitQueue();

  GShiftPoint(x, y);
  if (_hDensity) {
    coordSys = WinSetCoordinateSystem(kCoordinatesDouble);
  }
  WinSetForeColor(_colors[specP_->_fg]);
  while ((pP = GUnshiftPoint()) != NULL) {
    if (!_chk(pP->x, pP->y, _colors[specP_->_fg], _colors[specP_->_bg])) {
      /* needless to paint */
      continue;
    }
    for (l = pP->x ; l >= bl ; l--) {
      if (!_chk(l, pP->y, _colors[specP_->_fg], _colors[specP_->_bg])) {
	break;
      }
    }
    ++l;
    for (r = pP->x ; r < br ; r++) {
      if (!_chk(r, pP->y, _colors[specP_->_fg], _colors[specP_->_bg])) {
	break;
      }
    }
    --r;
    /* draw current line */
    if (_hrLib) {
      WinSetDrawWindow(_canvas);
      HRWinDrawLine(_hrLib, l, pP->y, r, pP->y);
      WinSetDrawWindow(cur);
      HRWinDrawLine(_hrLib, l, pP->y, r, pP->y);
    } else {
      WinSetDrawWindow(_canvas);
      WinDrawLine(l, pP->y, r, pP->y);
      WinSetDrawWindow(cur);
      WinDrawLine(l, pP->y, r, pP->y);
    }
    for (wx = l ; wx <= r ; wx++) {
      uf = df = false;
      /* scan upper line */
      wy = pP->y - 1;
      if (wy >= bu) {
	uf = _chk(wx, wy, _colors[specP_->_fg], _colors[specP_->_bg]);
	if (uf) {
	  if (wx + 1 <= r && !_chk(wx + 1, wy,
				   _colors[specP_->_fg],
				   _colors[specP_->_bg])) {
	    /* found right edge */
	    GShiftPoint(wx, wy);
	    uf = false;
	  } else if (wx == r) {
	    GShiftPoint(wx, wy);
	  }
	}
      }
      /* scan lower line */
      wy = pP->y + 1;
      if (wy < bd) {
	df = _chk(wx, wy, _colors[specP_->_fg], _colors[specP_->_bg]);
	if (df) {
	  if (wx + 1 <= r && !_chk(wx + 1, wy,
				   _colors[specP_->_fg],
				   _colors[specP_->_bg])) {
	    /* found right edge */
	    GShiftPoint(wx, wy);
	    df = false;
	  } else if (wx == r) {
	    GShiftPoint(wx, wy);
	  }
	}
      }
    }
  }
  if (_hDensity) {
    WinSetCoordinateSystem(coordSys);
  }
}

void
GPaint(RectanglePtr clipP_, Coord x_, Coord y_, GColorName f_, GColorName b_)
{
  GPaintSpecType spec;
  spec._clipP  = clipP_;
  spec._x      = x_;
  spec._y      = y_;
  spec._fg     = f_;
  spec._bg     = b_;
  spec._bgBmpP = NULL;
  GPaintSpec(&spec);
}

void
GDrawLine(RectanglePtr clipP_,
	  Coord x0_, Coord y0_, Coord x1_, Coord y1_, GColorName c_)
{
  RectangleType cur;
  WinHandle     draw, windows[2];
  UInt16        i;
  UInt16        coordSys;

  if (_hDensity) {
    coordSys = WinSetCoordinateSystem(kCoordinatesDouble);
  }
  draw = WinGetDrawWindow();
  WinSetForeColor(_colors[c_]);
  windows[0] = _canvas;
  windows[1] = draw;
  for (i = 0 ; i < 2; i++) {
    WinSetDrawWindow(windows[i]);
    if (_hrLib) {
      if (clipP_) {
	HRWinGetClip(_hrLib, &cur);
	HRWinSetClip(_hrLib, clipP_);
      }
      HRWinDrawLine(_hrLib, x0_, y0_, x1_, y1_);
      if (clipP_) {
	HRWinSetClip(_hrLib, &cur);
      }
    } else {
      if (clipP_) {
	WinGetClip(&cur);
	WinSetClip(clipP_);
      }
      WinDrawLine(x0_, y0_, x1_, y1_);
      if (clipP_) {
	WinSetClip(&cur);
      }
    }
  }
  if (_hDensity) {
    WinSetCoordinateSystem(coordSys);
  }
}

GColorName
GGetPixel(Coord x0_, Coord y0_)
{
  UInt16           i, coordSys;
  IndexedColorType c;
  GColorName       r = gColorInvalid;
  if (_hDensity) {
    coordSys = WinSetCoordinateSystem(kCoordinatesDouble);
  }
  c = GFetchPixel(x0_, y0_);
  for (i = 0 ; i < gColorNameCandidate ; i++) {
    if (_colors[i] == c) {
      r = i;
      break;
    }
  }
  if (_hDensity) {
    WinSetCoordinateSystem(coordSys);
  }
  return r;
}

void
GDrawPixel(RectanglePtr clipP_, Coord x0_, Coord y0_, GColorName c_)
{
  UInt16 coordSys;

  if (_hDensity) {
    coordSys = WinSetCoordinateSystem(kCoordinatesDouble);
  }
  WinSetForeColor(_colors[c_]);
  GSetPixel(x0_, y0_, _colors[c_]);
  if (_hrLib) {
    HRWinDrawPixel(_hrLib, x0_, y0_);
  } else {
    WinDrawPixel(x0_, y0_);
  }
  if (_hDensity) {
    WinSetCoordinateSystem(coordSys);
  }
}

void
GDrawRectangle(RectanglePtr rP_, GColorName c_)
{
  UInt16        coordSys;
  WinHandle     draw = WinGetDrawWindow();
  
  if (_hDensity) {
    coordSys = WinSetCoordinateSystem(kCoordinatesDouble);
  }
  WinSetForeColor(_colors[c_]);
  if (_hrLib) {
    WinSetDrawWindow(_canvas);
    HRWinDrawRectangle(_hrLib, rP_, 0);
    WinSetDrawWindow(draw);
    HRWinDrawRectangle(_hrLib, rP_, 0);
  } else {
    WinSetDrawWindow(_canvas);
    WinDrawRectangle(rP_, 0);
    WinSetDrawWindow(draw);
    WinDrawRectangle(rP_, 0);
  }
  if (_hDensity) {
    WinSetCoordinateSystem(coordSys);
  }
}

void
GDrawChars(Boolean small_,
	   Char *strP_, UInt16 len_, Coord x_, Coord y_, GColorName c_)
{
  UInt16    i;
  UInt16    coordSys;
  WinHandle windows[2];

  if (_hDensity) {
    coordSys = WinSetCoordinateSystem(kCoordinatesDouble);
  }
  WinSetTextColor(_colors[c_]);

  windows[0] = _canvas;
  windows[1] = WinGetDrawWindow();
  if (strP_[len_ - 1] < ' ') {
    --len_;
  }
  for(i = 0 ; i < 2 ; i++) {
    WinSetDrawWindow(windows[i]);
    if (_hDensity) {
      if (small_){
	HDWinDrawChars(strP_, len_, x_, y_);
      } else {
	WinDrawChars(strP_, len_, x_, y_);
      }
    } else {
      HRWinDrawChars(_hrLib, strP_, len_, x_, y_);
    }
  }
  if (_hDensity) {
    WinSetCoordinateSystem(coordSys);
  }
}

void
GPaletteChange(GColorName c_)
{
  UInt16       i;
  RGBColorType *mapP, rgb;
  MemHandle    hRGB;

  hRGB = MemHandleNew(sizeof(RGBColorType) * 256);
  mapP = (RGBColorType*)MemHandleLock(hRGB);
  WinPalette(winPaletteGet, 0, 256, mapP);
  if (c_ != gColorInvalid) {
    WinIndexToRGB(_colors[c_], &rgb);
  }
  for (i = gColorBlue ; i < gColorWhite ; i++) {
    mapP[_colors[i]] = (c_ == gColorInvalid) ? _defrgb[i] : rgb;
  }
  WinPalette(winPaletteSet, 0, 256, mapP);
  MemHandleUnlock(hRGB);
  MemHandleFree(hRGB);
}

FontID
GGetFont(void)
{
  return (!_hDensity && _hrLib) ? HRFntGetFont(_hrLib) : FntGetFont();
}

FontID
GSetFont(FontID id_)
{
  return (!_hDensity && _hrLib) ? HRFntSetFont(_hrLib, id_) : FntSetFont(id_);
}

void
GFldModifyField(FieldPtr fldP)
{
  if (_hDensity || _hrLib) {
    FldSetUsable(fldP, false);
  }
}

void
GFldDrawField(FieldPtr fldP)
{
  UInt16        lines, pos, offset, maxLength;
  UInt16        h, w, l;
  FontID        curFont = stdFont, fldFont;
  Char          *textP;
  RectangleType b;
  Boolean       small = false;

  if (!_hDensity && _hrLib == 0) {
    FldDrawField(fldP);
    return;
  }
  WinPushDrawState();
  if (_hDensity) {
    WinSetCoordinateSystem(kCoordinatesDouble);
  }
  FldGetBounds(fldP, &b);
  if (_hDensity || _hrLib) {
    b.topLeft.x <<= 1;
    b.topLeft.y <<= 1;
    b.extent.x <<= 1;
    b.extent.y <<= 1;
  }
  fldFont = FldGetFont(fldP);
  textP = FldGetTextPtr(fldP);
  maxLength = FldGetTextLength(fldP);
  if (textP) {
    GFldEraseField(fldP);
    if (_hDensity) {
      if (fldFont & fntAppFontCustomBase) {
	fldFont -= fntAppFontCustomBase;
	small    = true;
      }
      curFont = FntSetFont(fldFont);
    } else if (_hrLib) {
      FontID hrFont;
      if (fldFont & fntAppFontCustomBase) {
	/* use small font */
	fldFont -= fntAppFontCustomBase;
	small    = true;
      }
      hrFont = _fntTable[fldFont + ((small) ? 0 : 4)];
      if (fldFont == largeBoldFont) {
	hrFont = _fntTable[(small) ? 3 : 7];
      }
      curFont = HRFntSetFont(_hrLib, hrFont);
    }
    h = FntLineHeight();
    w = b.extent.x;
    if (small) {
      if (_hDensity) {
	h /= 2;
	w *= 2;
      }
    }
    if (!_hDensity && _hrLib && _hrVer >= HR_VERSION_SUPPORT_FNTSIZE) {
      h = HRFntLineHeight(_hrLib);
    }
    lines = FldGetVisibleLines(fldP);
    if (_hDensity) {
      lines = b.extent.y / h;
    }
    offset = FldGetScrollPosition(fldP);
    for (pos = 0 ; pos < lines ; pos++) {
      l = FldWordWrap(&textP[offset], w);
      GDrawChars(small, &textP[offset],
		 l, b.topLeft.x, b.topLeft.y + pos * h, gColorBlue);
      offset += l;
      if (offset >= maxLength) {
	break;
      }
    }
    if (_hDensity) {
      FntSetFont(curFont);
    } else if (_hrLib) {
      HRFntSetFont(_hrLib, curFont);
    }
  }
  WinPopDrawState();
}

void
GFldEraseField(FieldPtr fldP)
{
  RectangleType b;
  if (!_hDensity && _hrLib == 0) {
    FldEraseField(fldP);
    return;
  }
  FldGetBounds(fldP, &b);
  b.topLeft.x <<= 1;
  b.topLeft.y <<= 1;
  b.extent.x <<= 1;
  b.extent.y <<= 1;
  if (_hDensity) {
    WinPushDrawState();
    WinSetCoordinateSystem(kCoordinatesDouble);
    WinEraseRectangle(&b, 0);
    WinPopDrawState();
  } else if (_hrLib) {
    HRWinEraseRectangle(_hrLib, &b, 0);
  }
}

void
GGetDisplayExtent(Coord *xP, Coord *yP, Boolean hd)
{
  WinPushDrawState();
  if (_hDensity) {
    WinSetCoordinateSystem((hd) ? kCoordinatesDouble : kCoordinatesStandard);
  }
  WinGetDisplayExtent(xP, yP);
  WinPopDrawState();
}

UInt16
GScale(void)
{
  return (_hDensity || _hrLib) ? 2 : 1;
}
