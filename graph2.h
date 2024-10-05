/*
 * Graphics routine for High-High-School Adventure.
 *  Copyright(c)2003 ZOBplus hiro <hiro@zob.ne.jp>
 */

#ifndef GRAPH2_H
#define GRAPH2_H

typedef enum GColorName {
  gColorInvalid = -1,
  gColorBlack,
  gColorBlue,
  gColorRed,
  gColorMagenta,
  gColorGreen,
  gColorCyan,
  gColorYellow,
  gColorWhite,
  gColorDarkGray,
  gColorDarkBlue,
  gColorDarkRed,
  gColorDarkMagenta,
  gColorDarkGreen,
  gColorDarkCyan,
  gColorDarkYellow,
  gColorGray,

  gColorNameCandidate
} GColorName;

typedef struct {
  RectanglePtr _clipP;
  Coord        _x, _y;
  GColorName   _fg;
  GColorName   _bg;     /* _bg and _bgBmpP must use exclusively   */
  BitmapPtr    _bgBmpP; /* _bgBmpP must be NULL when _bg is valid */
} GPaintSpecType;
typedef GPaintSpecType *GPaintSpecPtr;

Boolean    GInit(void);
Boolean    GFinalize(void);

void       GPaint(RectanglePtr, Coord, Coord, GColorName, GColorName);
void       GPaintSpec(GPaintSpecPtr);
void       GDrawLine(RectanglePtr, Coord, Coord, Coord, Coord, GColorName);
void       GDrawPixel(RectanglePtr, Coord, Coord, GColorName);
void       GDrawRectangle(RectanglePtr, GColorName);
void       GDrawChars(Boolean, Char*, UInt16, Coord, Coord, GColorName);
void       GPaletteChange(GColorName);
void       GChromakeyPaint(RectanglePtr, UInt8*, Boolean);
GColorName GGetPixel(Coord, Coord);
FontID     GGetFont(void);
FontID     GSetFont(FontID);

void       GFldModifyField(FieldPtr);
void       GFldDrawField(FieldPtr);
void       GFldEraseField(FieldPtr);

void       GGetDisplayExtent(Coord*, Coord*, Boolean);

UInt16     GScale(void);

#endif /* GRAPH2_H */
