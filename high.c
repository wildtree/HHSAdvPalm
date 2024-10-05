/*
 * High-High School Adventure
 *   Copyright(c)2002-2003 ZOBplus hiro <hiro@zob.ne.jp>
 *   Copyright(c)ZOBplus
 */

#include <PalmOS.h>
#include <SonyCLIE.h>
#include <SmallFontSupport.h>
#ifdef USE_GRAPH2
#include "graph2.h"
#else /* USE_GRAPH2 */
#include "graph.h"
#endif /* USE_GRAPH2 */
#include "high.h"
#include "highrsc.h"

static ZamaMapType      map;
static ZamaUserDataType user;
static UInt8            object[0x200];
static ZamaPrefsType    prefs;

static Char             cmd[5];
static Char             obj[5];

static RectangleType    pict;
static UInt16           _silkr; /* silk control */
static UInt32           _silkVer;
static Boolean          _resized; /* resized */
static Boolean          _ignoreKey;

static UInt32           _startup;
static Boolean          _over;

/* tsPatch support */
static Boolean          tsPatch;
static UInt32           tinyFontID;
static UInt32           tinyBoldFontID;
static UInt32           smallFontID;
static UInt32           smallSymbolFontID;
static UInt32           smallSymbol11FontID;
static UInt32           smallSymbol7FontID;
static UInt32           smallLedFontID;
static UInt32           smallBoldFontID;

static
MemPtr
GetObjectPtr(UInt16 id)
{
  FormPtr form;
  form = FrmGetActiveForm();
  return FrmGetObjectPtr(form, FrmGetObjectIndex(form, id));
}

static
DmOpenRef
OpenDatabase(Char *name_)
{
  LocalID     dbID;
  DmOpenRef   dbP;
  Err         err;
  UInt16      cardNo;

  if ((err = SysCurAppDatabase(&cardNo, &dbID)) != errNone) {
    cardNo = 0;
  }
  if ((dbID = DmFindDatabase(cardNo, name_)) == 0) {
    /* error */
    return NULL;
  }
  if ((dbP = DmOpenDatabase(cardNo, dbID, dmModeReadOnly)) == 0) {
    /* error */
    return NULL;
  }
  return dbP;
}

static
Boolean
SaveGame(UInt16 fileno_)
{
  LocalID     dbID;
  DmOpenRef   dbP;
  MemHandle   rec;
  UInt16      err, i, index, attr, version;
  MemPtr      p;
  Boolean     res = true;
  UInt16      cardNo;

  if ((err = SysCurAppDatabase(&cardNo, &dbID)) != errNone) {
    cardNo = 0;
  }
  while ((dbID = DmFindDatabase(cardNo, "ZAMAuser")) == 0) {
    /* create database */
    err = DmCreateDatabase(cardNo, "ZAMAuser", ZAMA_CREATOR_ID, 'data', false);
    if (err) {
      FrmAlert(ZAMA_SAVE_ALERT);
      return false;
    }
  } 
  DmDatabaseInfo(cardNo, dbID, NULL, &attr, &version,
		 NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  attr |= dmHdrAttrBackup;
  version = 0x100; /* version 1.00 */
  DmSetDatabaseInfo(cardNo, dbID, NULL, &attr, &version,
		    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  if ((dbP = DmOpenDatabase(cardNo, dbID, dmModeReadWrite)) == 0) {
    /* error */
    FrmAlert(ZAMA_SAVE_ALERT);
    return false;
  }
  for (i = 0 ; i < 4 ; i++) {
    if (DmQueryRecord(dbP, i) == NULL) {
      if ((rec = DmNewHandle(dbP, sizeof(ZamaUserDataType))) == NULL) {
	FrmAlert(ZAMA_SAVE_ALERT);
	res = false;
	goto _done;
      }
      index  = i;
      if (DmAttachRecord(dbP, &index, rec, NULL) != errNone) {
	FrmAlert(ZAMA_SAVE_ALERT);
	res = false;
	goto _done;
      }
    }
  }
  if ((rec = DmGetRecord(dbP, fileno_)) == NULL) {
    FrmAlert(ZAMA_SAVE_ALERT);
    res = false;
    goto _done;
  }
  p = MemHandleLock(rec);

  DmWrite(p, 0, &user, sizeof(ZamaUserDataType));

  MemHandleUnlock(rec);
  DmReleaseRecord(dbP, fileno_, true);
  if (fileno_ != 0) {
    prefs._lastSaved = fileno_;
  }
 _done:;
  DmCloseDatabase(dbP);
  return res;
}

static
Boolean
LoadGame(UInt16 fileno_)
{
  DmOpenRef   dbP;
  MemHandle   rec;
  Char        sid[16];
  MemPtr      p;
  Boolean     res = true;

  if ((dbP = OpenDatabase("ZAMAuser")) == NULL) {
    FrmCustomAlert(ZAMA_NODATA_ALERT, StrIToA(sid, fileno_), NULL, NULL);
    return false;
  }
  if ((rec = DmQueryRecord(dbP, fileno_)) == NULL) {
    FrmCustomAlert(ZAMA_NODATA_ALERT, StrIToA(sid, fileno_), NULL, NULL);
    res = false;
    goto _done;
  }
  p = MemHandleLock(rec);

  MemMove((MemPtr)&user, p, sizeof(ZamaUserDataType));

  MemHandleUnlock(rec);
  prefs._lastSaved = fileno_;
 _done:;
  DmCloseDatabase(dbP);
  return res;
}

#ifdef USE_GRAPH2
static
FontID
ZamaFontSelect(FontID curFont_)
{
  FormPtr       formP;
  UInt16        ret;
  Coord         x, y;
  WinHandle     hWin;
  RectangleType bounds;
  FontID        font;
  UInt16        small, fnt;

  formP = FrmInitForm(ZAMA_FONT_FORM);
  GGetDisplayExtent(&x, &y, false);
  if (y > 160) {
    /* silk turned off */
    hWin = FrmGetWindowHandle(formP);
    WinGetBounds(hWin, &bounds);
    bounds.topLeft.y += stdSilkHeight;
    if (bounds.topLeft.y + bounds.extent.y >= y) {
      bounds.topLeft.y = y - bounds.extent.y;
    }
    WinSetBounds(hWin, &bounds);
  }
  small = FrmGetObjectIndex(formP, ZAMA_SMALL_FONT);
  font = curFont_;
  if (font & fntAppFontCustomBase) {
    font -= fntAppFontCustomBase;
    CtlSetValue(FrmGetObjectPtr(formP, small), true);
  }
  switch (font) {
  case stdFont:       fnt = ZAMA_STD_FONT;        break;
  case boldFont:      fnt = ZAMA_BOLD_FONT;       break;
  case largeFont:     fnt = ZAMA_LARGE_FONT;      break;
  case largeBoldFont: fnt = ZAMA_LARGE_BOLD_FONT; break;
  default:
    CtlSetValue(FrmGetObjectPtr(formP, small), true);
    if (font == tinyFontID) {
      fnt = ZAMA_STD_FONT;
    } else if (font == tinyBoldFontID) {
      fnt = ZAMA_BOLD_FONT;
    } else if (font == smallFontID) {
      fnt = ZAMA_LARGE_FONT;
    } else if (font == smallBoldFontID) {
      fnt = ZAMA_LARGE_BOLD_FONT;
    } else {
      fnt = ZAMA_STD_FONT; 
    }
    break;
  }
  fnt = FrmGetObjectIndex(formP, fnt);
  CtlSetValue(FrmGetObjectPtr(formP, fnt), true);
  FrmUpdateForm(ZAMA_FONT_FORM, frmRedrawUpdateCode);
  ret = FrmDoDialog(formP);
  fnt = FrmGetControlGroupSelection(formP, ZAMA_FONT_SELECT);
  if (fnt == frmNoSelectedControl) {
    ret = ZAMA_BUTTON_CANCEL;
  }
  if (fnt == FrmGetObjectIndex(formP, ZAMA_STD_FONT)) {
    fnt = stdFont;
  } else if (fnt == FrmGetObjectIndex(formP, ZAMA_BOLD_FONT)) {
    fnt = boldFont;
  } else if (fnt == FrmGetObjectIndex(formP, ZAMA_LARGE_FONT)) {
    fnt = largeFont;
  } else if (fnt == FrmGetObjectIndex(formP, ZAMA_LARGE_BOLD_FONT)) {
    fnt = largeBoldFont;
  } else {
    ret = ZAMA_BUTTON_CANCEL;
  }
  if (CtlGetValue(FrmGetObjectPtr(formP, small))) {
    if (tsPatch) {
      switch (fnt) {
      case stdFont:      fnt = tinyFontID; break;
      case boldFont:     fnt = tinyBoldFontID; break;
      case largeFont:    fnt = smallFontID; break;
      case largeBoldFont: fnt = smallBoldFontID; break;
      }
    } else {
      fnt += fntAppFontCustomBase;
    }
  }
  FrmDeleteForm(formP);
  return (ret == ZAMA_BUTTON_OK) ? fnt : curFont_;
}
#endif /* USE_GRAPH2 */

static
Boolean
ZamaSndPlay(UInt16 id)
{
  UInt16  volRefNum;
  Err     err;
  Char    *pathFmt =  = "/Palm/PROGRAMS/high/%d.wav";
  Char    pathName[1024];
  FileRef dbFileRef;
  UInt32  size;
  SndPtr  bufP;
  UInt32  vIterator;
  if (prefs._sound) {
    /* play sound */
    StrPrintF(pathName, pathFmt, id);
    vIterator = vfsIteratorStart;
     err = VFSVokumeEnumerate(&volRefNum, &vIterator);
    if (err != errNone) {
      return false;
    }
    if (VFSFileOpen(volRefNum, pathName, vfsModeRead, &dbFileRef) != errNone) {
      return false;
    }
    VFSFileSize(dbFileRef, &size);
  }
  return true;
}

static
void
VOSplit(Char *srcP_)
{
  Char   *p;
  UInt16  i;

  StrCopy(cmd, "!!!!");
  StrCopy(obj, "!!!!");
  for (p = srcP_ ; *p && *p == ' ' ; p++); /* skip heading space */
  for (i = 0 ; *p && *p != ' ' && i < 4 ; i++) {
    cmd[i] = 1 + *p++;
  }
  for ( ; *p && *p != ' ' ; p++); /* skip until space */
  for ( ; *p && *p == ' ' ; p++); /* skip space */
  for (i = 0 ; *p && *p != ' ' && i < 4 ; i++) {
    obj[i] = 1 + *p++;
  }
}

static
UInt16
SearchWord(Char *word_, Boolean verb_)
{
  DmOpenRef   dbP;
  MemHandle   rec;
  ZamaWordPtr p;
  UInt16      index;
  UInt16      i, n;

  if (word_ == NULL || StrCompare("!!!!", word_) == 0) {
    return 0;
  }
  if ((dbP = OpenDatabase("ZAMAdict")) == NULL) {
    FrmCustomAlert(ZAMA_FATAL_ALERT, "辞書", NULL, NULL);
    return 0;
  }
  index = (verb_) ? 0 : 1;
  n = 0;
  if ((rec = DmQueryRecord(dbP, index)) == NULL) {
    FrmCustomAlert(ZAMA_FATAL_ALERT, "辞書", NULL, NULL);
    goto _done;
  }
  p = (ZamaWordPtr)MemHandleLock(rec);
  for (i = 0 ; p[i].word[0] ; i++) {
    if (StrNCaselessCompare(word_, p[i].word, 4) == 0) {
      n = p[i].number;
      break;
    }
  }
  MemHandleUnlock(rec);
 _done:;
  DmCloseDatabase(dbP);
  return n;
}

static
MemHandle
SearchFixedMessage(UInt16 mesgId_)
{
  ZamaMesgPtr  p = NULL;
  UInt16       i;
  DmOpenRef    dbP;
  MemHandle    rec, mesg;
  ZamaMesgPtr  mesgP;

  if ((dbP = OpenDatabase("ZAMAdict")) == NULL) {
    /* error */
    FrmCustomAlert(ZAMA_FATAL_ALERT, "辞書", NULL, NULL);
    return 0;
  }
  mesg = 0;
  if ((rec = DmQueryRecord(dbP, 2)) == NULL) {
    FrmCustomAlert(ZAMA_FATAL_ALERT, "辞書", NULL, NULL);
    goto _done;
  }
  mesgId_ &= 0x7f;
  p = (ZamaMesgPtr)MemHandleLock(rec);
  for (i = 0 ; p->len > 0 && i < mesgId_ - 1 ; i++) {
    p = (ZamaMesgPtr)(p->mesg + p->len);
  }
  if (p && p->len) {
    mesg = MemHandleNew(sizeof(ZamaMesgType) + p->len - 1);
    mesgP = (ZamaMesgPtr)MemHandleLock(mesg);
    MemMove(mesgP, p, sizeof(ZamaMesgType) + p->len - 1);
    MemHandleUnlock(mesg);
  }

  MemHandleUnlock(rec);
 _done:;
  DmCloseDatabase(dbP);
  return mesg;
}

static
MemHandle
SearchMapMessage(UInt16 cmdN_, UInt16 objN_)
{
  ZamaMesgPtr      p = NULL;
  int              i;
  int              n = 0;
  ZamaReactionType mp;
  UInt8           *rp = map.reaction;
  MemHandle        mesg;
  ZamaMesgPtr      mesgP;

  cmdN_ = (cmdN_ << 8) | objN_;
  for (i = 0 ; *rp ; i++) {
    mp.action = (rp[0] << 8) | rp[1];
    mp.number = rp[2];
    if (cmdN_ == mp.action) {
      p = (ZamaMesgPtr)map.mesg;
      for (n = 0 ; p->len > 0 && n < mp.number - 1 ; n++) {
	p = (ZamaMesgPtr)(p->mesg + p->len);
      }
      break;
    }
    rp += 3;
  }
  mesg = 0;
  if (p && p->len) {
    mesg = MemHandleNew(sizeof(ZamaMesgType) + p->len - 1);
    mesgP = (ZamaMesgPtr)MemHandleLock(mesg);
    MemMove(mesgP, p, sizeof(ZamaMesgType) + p->len - 1);
    MemHandleUnlock(mesg);
  } else {
    cmdN_ >>= 8; /* restore Command ID */
    i = 0;
    if (cmdN_ <= 0x0e && cmdN_ >= 0x0b) {
      i = cmdN_ - 8; 
    } else if (cmdN_ == 7 || cmdN_ == 8) {
      i = cmdN_ - 6;
    } else if (cmdN_ >= 0x11 && cmdN_ <= 0x18) {
      i = cmdN_ - 9;
    } else if (cmdN_ == 0x1c || cmdN_ == 0x1d) {
      i = cmdN_ - 12;
    } else if (cmdN_ == 0x20 || cmdN_ == 0x21) {
      i = cmdN_ - 14;
    } else if (cmdN_ == 0x26) {
      i = 0x14;
    } else if (cmdN_ == 0x28 || cmdN_ == 0x2f) {
      i = 0x15;
    } else if (cmdN_ == 0x29) {
      i = 7;
    } else if (objN_ == 0) {
      i = 0x30 + (SysRandom(0) % 4);
    }
    if (i) {
      mesg = SearchFixedMessage(i);
    }
  }
  return mesg;
}

static
void
ScrollMessageField(void)
{
  Int16        lines, pos, min, max, page;
  UInt16       scrollPos, textHeight, fieldHeight;
  FieldPtr     fldP;
  ScrollBarPtr scrP;
  fldP = GetObjectPtr(ZAMA_MESSAGE_FIELD);
  scrP = GetObjectPtr(ZAMA_SCROLL_BAR);
  SclGetScrollBar(scrP, &pos, &min, &max, &page);
  FldGetScrollValues(fldP, &scrollPos, &textHeight, &fieldHeight);
  lines = (pos - scrollPos) /** GScale()*/;
  if (lines > 0) {
    FldScrollField(fldP, lines, winDown);
  } else if (lines < 0) {
    FldScrollField(fldP, -lines, winUp);
  }
  FldSetDirty(fldP, true);
#ifdef USE_GRAPH2
  if (tsPatch) {
    FldDrawField(fldP);
  } else {
    GFldDrawField(fldP);
  }
#else /* USE_GRAPH2 */
  ZamaFldDrawField(fldP);
#endif /* USE_GRAPH2 */
}

static
void
UpdateMessageFieldScrollbar(void)
{
  UInt16       scrollPos, textHeight, fieldHeight;
  Int16        maxValue;
  FieldPtr     fldP;
  ScrollBarPtr scrP;

  fldP = GetObjectPtr(ZAMA_MESSAGE_FIELD);
  scrP = GetObjectPtr(ZAMA_SCROLL_BAR);
  maxValue = 0;
  FldGetScrollValues(fldP,&scrollPos, &textHeight, &fieldHeight);
  /* fieldHeight *= GScale(); */
  if (textHeight > fieldHeight) {
    maxValue = (textHeight - fieldHeight) + FldGetNumberOfBlankLines(fldP);
  } else if (scrollPos) {
    maxValue = scrollPos;
  }
  SclSetScrollBar(scrP, scrollPos, 0, maxValue, fieldHeight - 1);
}

static
void
CleanMessageField(void)
{
  FieldPtr  fldP;
  MemHandle hText;

  fldP = GetObjectPtr(ZAMA_MESSAGE_FIELD);
#ifdef USE_GRAPH2
  if (tsPatch) {
    FldEraseField(fldP);
  } else {
    GFldEraseField(fldP);
  }
#else /* USE_GRAPH2 */
  ZamaFldEraseField(fldP);
#endif /* USE_GRAPH2 */
  if ((hText = FldGetTextHandle(fldP)) != NULL) {
    FldSetTextHandle(fldP, NULL);
    MemHandleFree(hText);
    FldSetDirty(fldP, true);
  }
  UpdateMessageFieldScrollbar();
#ifdef USE_GRAPH2
  if (tsPatch) {
    FldDrawField(fldP);
  } else {
    GFldDrawField(fldP);
  }
#else /* USE_GRAPH2 */
  ZamaFldDrawField(fldP);
#endif /* USE_GRAPH2 */
}

static
void
PutMessageToField(Char *strP_, UInt16 len_)
{
  FieldPtr      fldP;
  MemHandle     hText;
  Char          *textP;
  UInt16        max;

  fldP  = GetObjectPtr(ZAMA_MESSAGE_FIELD);
  max   = FldGetTextLength(fldP);
  hText = FldGetTextHandle(fldP);
  FldSetTextHandle(fldP, NULL);
  if (hText == NULL) {
    hText = MemHandleNew(len_ + 1);
    textP = MemHandleLock(hText);
  } else {
    MemHandleResize(hText, max + len_ + 2);
    textP = MemHandleLock(hText);
    textP[max++] = '\n'; /* add newline */
    textP[max]   = 0;
  }
  MemMove(&textP[max], strP_, len_);
  textP[max + len_] = 0;
  MemHandleUnlock(hText);

  FldSetTextHandle(fldP, hText);
  FldSetDirty(fldP, true);
#ifdef USE_GRAPH2
  if (tsPatch) {
    FldDrawField(fldP);
  } else {
    GFldDrawField(fldP);
  }
#else /* USE_GRAPH2 */
  ZamaFldDrawField(fldP);
#endif /* USE_GRAPH2 */
  UpdateMessageFieldScrollbar();
}

static
void
DrawMessage(UInt16 mesgId_, UInt16 cmdN_, UInt16 objN_)
{
  Char          *strP;
  UInt16        len;
  MemHandle     mesg;
  ZamaMesgPtr   mesgP;
  
  if (mesgId_ & 0x80) {
    mesg = SearchFixedMessage(mesgId_ & 0x7f);
  } else {
    if (mesgId_ == 1) {
      mesgP = (ZamaMesgPtr)map.mesg;
      mesg  = MemHandleNew(sizeof(ZamaMesgType) + mesgP->len - 1);
      MemMove(MemHandleLock(mesg), mesgP, sizeof(ZamaMesgType) + mesgP->len - 1);
      MemHandleUnlock(mesg);
    } else {
      mesg = SearchMapMessage(cmdN_, objN_);
    }
  }
  strP = "ダメ";
  len  = StrLen(strP);
  if (mesg) {
    mesgP = (ZamaMesgPtr)MemHandleLock(mesg);
    strP  = mesgP->mesg;
    len   = mesgP->len;
  }
  PutMessageToField(strP, len);
  if (mesg) {
    MemHandleUnlock(mesg);
    MemHandleFree(mesg);
  }
}

static
Boolean
LoadInitData(void)
{
  DmOpenRef dbP;
  MemHandle rec;
  MemPtr    p;
  Boolean   res = true;

  if ((dbP = OpenDatabase("ZAMAinit")) == NULL) {
    /* error */
    FrmCustomAlert(ZAMA_FATAL_ALERT, "初期データ", NULL, NULL);
    return false;
  }
  if ((rec = DmQueryRecord(dbP, 0)) == NULL) {
    FrmCustomAlert(ZAMA_FATAL_ALERT, "初期データ", NULL, NULL);
    res = false;
    goto _done;
  }
  p   = (MemPtr)MemHandleLock(rec);
  MemMove(&user.vector, p, sizeof(user.vector));
  MemHandleUnlock(rec);

  if ((rec = DmQueryRecord(dbP, 1)) == NULL) {
    FrmCustomAlert(ZAMA_FATAL_ALERT, "初期データ", NULL, NULL);
    res = false;
    goto _done;
  }
  p   = (MemPtr)MemHandleLock(rec);
  MemMove(&user.place, p, sizeof(user.place));
  MemHandleUnlock(rec);

  if ((rec = DmQueryRecord(dbP, 2)) == NULL) {
    FrmCustomAlert(ZAMA_FATAL_ALERT, "初期データ", NULL, NULL);
    res = false;
    goto _done;
  }
  p   = (MemPtr)MemHandleLock(rec);
  MemMove(&user.fact, p, sizeof(user.fact));
  MemHandleUnlock(rec);
 _done:;
  DmCloseDatabase(dbP);
  return res;
}

static
Boolean
LoadMapData(UInt16 id)
{
  DmOpenRef dbP;
  MemHandle rec;
  MemPtr    p;
  Boolean   res = true;

  if ((dbP = OpenDatabase("ZAMAmap")) == NULL) {
    /* error */
    FrmCustomAlert(ZAMA_FATAL_ALERT, "地図", NULL, NULL);
    return false;
  }
  if ((rec = DmQueryRecord(dbP, id)) == NULL) {
    FrmCustomAlert(ZAMA_FATAL_ALERT, "地図", NULL, NULL);
    res = false;
    goto _done;
  }
  p   = (MemPtr)MemHandleLock(rec);
  MemMove(&map, p, sizeof(ZamaMapType));
  MemHandleUnlock(rec);
 _done:;
  DmCloseDatabase(dbP);
  return true;
}

static
Boolean
LoadObjectData(UInt16 id)
{
  DmOpenRef dbP;
  MemHandle rec;
  MemPtr    p;
  Boolean   res = true;

  if ((dbP = OpenDatabase("ZAMAobj")) == NULL) {
    /* error */
    FrmCustomAlert(ZAMA_FATAL_ALERT, "もの画像", NULL, NULL);
    return false;
  }
  if ((rec = DmQueryRecord(dbP, id)) == NULL) {
    FrmCustomAlert(ZAMA_FATAL_ALERT, "もの画像", NULL, NULL);
    res = false;
    goto _done;
  }
  p   = (MemPtr)MemHandleLock(rec);
  MemMove(object, p, 0x200);
  MemHandleUnlock(rec);
 _done:;
  DmCloseDatabase(dbP);
  return true;
}

static
UInt16
WinDrawOutline(RectanglePtr bP, UInt8 *data, 
#ifdef USE_GRAPH2
	       GColorName c
#else /* USE_GRAPH2 */
	       ZamaColorName c
#endif /* USE_GRAPH2 */
	       )
{
  Coord  x0, y0, x1, y1;
  UInt16 i = 0;

  x0 = data[i++];
  y0 = data[i++];
  for (;;) {
    x1 = data[i++];
    y1 = data[i++];
    if (y1 == 0xff) {
      if (x1 == 0xff) {
	/* end of line data */
	break;
      }
      /* reset line */
      x0 = data[i++];
      y0 = data[i++];
      continue;
    }
#ifdef USE_GRAPH2
    GDrawLine(NULL,
	      bP->topLeft.x + x0, bP->topLeft.y + y0,
	      bP->topLeft.x + x1, bP->topLeft.y + y1, c);
#else /* USE_GRAPH2 */
    ZamaDrawLine(bP->topLeft.x + x0, bP->topLeft.y + y0,
		 bP->topLeft.x + x1, bP->topLeft.y + y1, c);
#endif /* USE_GRAPH2 */
    x0 = x1; y0 = y1;
  } 
  return i;
}

static
void
DrawObject(Boolean pre, UInt8 *data)
{
  UInt16           o = 0;
  Coord            xs, ys;
  UInt8            b, c;
  RectangleType    r;

  b  = data[o++];
  xs = (data[o++] / 2) + 32;
  ys = data[o++] + 32;
  RctSetRectangle(&r, xs, ys, 32 + 256 - xs, 32 + 152 - ys);
  WinPushDrawState();
#ifdef USE_GRAPH2
  o += WinDrawOutline(&r, &data[o], (pre)? gColorDarkYellow : b);
#else /* USE_GRAPH2 */
  o += WinDrawOutline(&r, &data[o], (pre)? DarkYellow : b);
#endif /* USE_GRAPH2 */
  xs = data[o++];
  ys = data[o++];
  while (xs != 0xff || ys != 0xff) {
    c = data[o++];
    /* in order to avoid paint loss */
#ifdef USE_GRAPH2
    GPaint(&r, xs, ys, (pre)? gColorDarkGray : c, (pre)? gColorDarkYellow : b);
#else /* USE_GRAPH2 */
    ZamaPaint(&r, xs, ys, (pre) ? DarkGray : c, (pre) ? DarkYellow : b);
#endif /* USE_GRAPH2 */
    xs = data[o++];
    ys = data[o++];
  }
  WinPopDrawState();
}

static
void
ObjectCheck(UInt16 mapID, Boolean msgOut)
{
  int              i;
  int              o = 0;

  for (i = 0 ; i < 12 ; i++) {
    if (user.place[i] == mapID && LoadObjectData(i + 1)) {
      /* draw object bitmap */
      o = 0;
      if (i == 1 /* Uniform */) {
	o = (user.fact[0] == 1) ? 0 : 256;
      }
      DrawObject(true,  &object[o]);
      DrawObject(false, &object[o]);
      if (msgOut) DrawMessage(0x96 + i, 0, 0);
    }
  }
}

static
void
DrawTeacher(void)
{
  Coord             x0, y0, x1, y1, oy;
  UInt16            i, j, k;
  UInt8             c = 0;
  UInt16            rep[] = { 18, 24, 2, 2, 2, 22, 9, 0xffff };
  UInt16            rep2[] = { 148, 14, 126, 6, 0, 0 };
  RectangleType     r, wr;

  oy = 32;
  if (LoadObjectData(15)) {
    RctSetRectangle(&r, 0, 0, 320, 320);
    y0 = 63;
    for (i = 0 ; i <= 172 ; i += 2) {
      x0 = object[i + 0];
      x1 = object[i + 1];
#ifdef USE_GRAPH2
    GDrawLine(NULL, x0, oy + y0, x1, oy + y0, gColorBlue);
#else /* USE_GRAPH2 */
    ZamaDrawLine(x0, oy + y0, x1, oy + y0, Blue);
#endif /* USE_GRAPH2 */
      ++y0;
    }
    for (j = 0 ; rep[j] != 0xffff ; j++) {
#ifdef USE_GRAPH2
      c  = object[i++];
      if (c == gColorRed) {
	c = gColorDarkRed;
      }
#else /* USE_GRAPH2 */
      c  = object[i++];
      if (c == Red) {
	c = DarkRed;
      }
#endif /* USE_GRAPH2 */
      x0 = object[i++];
      y0 = object[i++];
      for (k = 0 ; k <= rep[j] + 1 ; k++) {
	x1 = object[i++];
	y1 = object[i++];
#ifdef USE_GRAPH2
	GDrawLine(NULL, x0, oy + y0, x1, oy + y1, c);
#else /* USE_GRAPH2 */
	ZamaDrawLine(x0, oy + y0, x1, oy + y1, c);
#endif /* USE_GRAPH2 */
	x0 = x1;
	y0 = y1;
      }
      x0 = object[i++];
      y0 = object[i++];
      WinPushDrawState();
#ifdef USE_GRAPH2
      GPaint(&r, x0, oy + y0, c, c);
#else /* USE_GRAPH2 */
      ZamaPaint(&r, x0, oy + y0, c, c);
#endif /* USE_GRAPH2 */
      WinPopDrawState();
    }
    x0 = object[i++];
    y0 = object[i++];
#ifdef USE_GRAPH2
    GPaint(&r, x0, oy + y0, c, c);
#else /* USE_GRAPH2 */
    ZamaPaint(&r, x0, oy + y0, c, c);
#endif /* USE_GRAPH2 */
    for (j = 120 ; j < 124 ; j++) {
#ifdef USE_GRAPH2
      GDrawLine(NULL, j, oy + 64, j + 8, oy + 110, gColorYellow);
      GDrawLine(NULL, j + 9, oy + 110, j + 11, oy + 126, gColorWhite);
#else /* USE_GRAPH2 */
      ZamaDrawLine(j, oy + 64, j + 8, oy + 110, Yellow);
      ZamaDrawLine(j + 9, oy + 110, j + 11, oy + 126, White);
#endif /* USE_GRAPH2 */
    }
#ifdef USE_GRAPH2
    GDrawLine(NULL, 125, oy + 111, 133, oy + 109, gColorRed);
    GDrawLine(NULL, 133, oy + 109, 134, oy + 110, gColorRed);
    GDrawLine(NULL, 134, oy + 110, 125, oy + 112, gColorRed);
    GDrawLine(NULL, 125, oy + 112, 125, oy + 111, gColorRed);
    GDrawLine(NULL, 120, oy +  65, 123, oy +  64, gColorWhite);
    GDrawLine(NULL, 123, oy +  64, 121, oy +  62, gColorWhite);
    GDrawLine(NULL, 121, oy +  62, 120, oy +  65, gColorWhite);
    GPaint(&r, 122,oy +  63, gColorWhite, gColorWhite);
#else /* USE_GRAPH2 */
    ZamaDrawLine(125, oy + 111, 133, oy + 109, Red);
    ZamaDrawLine(133, oy + 109, 134, oy + 110, Red);
    ZamaDrawLine(134, oy + 110, 125, oy + 112, Red);
    ZamaDrawLine(125, oy + 112, 125, oy + 111, Red);
    ZamaDrawLine(120, oy +  65, 123, oy +  64, White);
    ZamaDrawLine(123, oy +  64, 121, oy +  62, White);
    ZamaDrawLine(121, oy +  62, 120, oy +  65, White);
    ZamaPaint(&r, 122,oy +  63, White, White);
#endif /* USE_GRAPH2 */
    for (k = 0 ; rep2[k + 1] != 0 ; k += 2) {
      for (j = 0, x0 = rep2[k] ; j < rep2[k + 1] ; j += 2) {
	y0 = object[i++];
	y1 = object[i++];
#ifdef USE_GRAPH2
	GDrawLine(NULL, x0, oy + y0, x0, oy + y1, gColorMagenta); ++x0;
	GDrawLine(NULL, x0, oy + y0, x0, oy + y1, gColorYellow);  ++x0;
#else /* USE_GRAPH2 */
	ZamaDrawLine(x0, oy + y0, x0, oy + y1, Magenta); ++x0;
	ZamaDrawLine(x0, oy + y0, x0, oy + y1, Yellow);  ++x0;
#endif /* USE_GRAPH2 */
	y0 = object[i++];
	y1 = object[i++];
#ifdef USE_GRAPH2
	GDrawLine(NULL, x0, oy + y0, x0, oy + y1, gColorWhite);   ++x0;
#else /* USE_GRAPH2 */
	ZamaDrawLine(x0, oy + y0, x0, oy + y1, White);   ++x0;
#endif /* USE_GRAPH2 */
      }
    }

#ifdef USE_GRAPH2
    RctSetRectangle(&wr, 149, oy + 78, 16, 6);
    GDrawRectangle(&wr, gColorBlack);
    RctSetRectangle(&wr, 150, oy + 79, 14, 4);
    GDrawRectangle(&wr, gColorWhite);
    RctSetRectangle(&wr, 156, oy + 78,  2, 6);
    GDrawRectangle(&wr, gColorBlack);
#else /* USE_GRAPH2 */
    RctSetRectangle(&wr, 149, oy + 78, 16, 6);
    ZamaDrawRectangle(&wr, Black);
    RctSetRectangle(&wr, 150, oy + 79, 14, 4);
    ZamaDrawRectangle(&wr, White);
    RctSetRectangle(&wr, 156, oy + 78,  2, 6);
    ZamaDrawRectangle(&wr, Black);
#endif /* USE_GRAPH2 */

    for (;;) {
      x1 = object[i++];
      y1 = object[i++];
      if (y1 == 0xff) {
	if (x1 == 0xff) {
	  break;
	}
	x0 = object[i++];
	y0 = object[i++];
	continue;
      }
      
#ifdef USE_GRAPH2
      GDrawLine(NULL, x0, oy + y0, x1, oy + y1, gColorBlack);
#else /* USE_GRAPH2 */
      ZamaDrawLine(x0, oy + y0, x1, oy + y1, Black);
#endif /* USE_GRAPH2 */
      x0 = x1;
      y0 = y1;
    }
  }
}

static
void
CleanMap(
#ifdef USE_GRAPH2
	 GColorName c_
#else /* USE_GRAPH2 */
	 ZamaColorName c_
#endif /* USE_GRAPH2 */
)
{
#ifdef USE_GRAPH2
  GDrawRectangle(&pict, c_);
#else /* USE_GRAPH2 */
  ZamaDrawRectangle(&pict, c_);
#endif /* USE_GRAPH2 */
}

static
void
DrawMap(Boolean putMesg_)
{
  UInt16        i = 0;
  Coord         x0, y0;
  UInt8         c;

  WinPushDrawState();
  i += map.graphic[i] * 3 + 1; /* skip HALF paint pattern */

  if (_startup == 0) {
    if (user.sys.n.mapID == 85 || user.sys.n.mapID == 84) {
      /* never draw */
#ifdef USE_GRAPH2
      CleanMap(gColorBlack);
#else /* USE_GRAPH2 */
      CleanMap(Black);
#endif /* USE_GRAPH2 */
      DrawMessage(0xcc, 0, 0);
      goto _done;
    }
  }

#ifdef USE_GRAPH2
  CleanMap(gColorBlue);
  /* draw outline */
  i += WinDrawOutline(&pict, &map.graphic[i], gColorWhite);
#else /* USE_GRAPH2 */
  CleanMap(Blue);
  /* draw outline */
  i += WinDrawOutline(&pict, &map.graphic[i], White);
#endif /* USE_GRAPH2 */
  /* paint */
  x0 = map.graphic[i++];
  y0 = map.graphic[i++];
  while (x0 != 0xff || y0 != 0xff) {
    c = map.graphic[i++];
#ifdef USE_GRAPH2
    GPaint(&pict, x0, y0, c, gColorWhite);
#else /* USE_GRAPH2 */
    ZamaPaint(&pict, x0, y0, c, White);
#endif /* USE_GRAPH2 */
    x0 = map.graphic[i++];
    y0 = map.graphic[i++];
  }
  if (map.graphic[i] == 0xff && map.graphic[i + 1] == 0xff) {
    i += 2;
  } else {
#ifdef USE_GRAPH2
    i += WinDrawOutline(&pict, &map.graphic[i], gColorWhite);
#else /* USE_GRAPH2 */
    i += WinDrawOutline(&pict, &map.graphic[i], White);
#endif /* USE_GRAPH2 */
  }
  if (map.graphic[i] == 0xff && map.graphic[i + 1] == 0xff) {
    i += 2;
  } else {
#ifdef USE_GRAPH2
    i += WinDrawOutline(&pict, &map.graphic[i], gColorBlack);
#else /* USE_GRAPH2 */
    i += WinDrawOutline(&pict, &map.graphic[i], Black);
#endif /* USE_GRAPH2 */
  }
  if (map.graphic[0]) {
#ifdef USE_GRAPH2
    GChromakeyPaint(&pict, &map.graphic[0], prefs._linedraw);
#else /* USE_GRAPH2 */
    ZamaChromakeyPaint(&pict, &map.graphic[0], prefs._linedraw);
#endif /* USE_GRAPH2 */
  }
  /* put message #0 */
  if (putMesg_) DrawMessage(1, 0, 0);
 _done:;
  WinPopDrawState();
}

static
void
GameInit(void)
{
  FormPtr    formP;
  ControlPtr btnP, chkP;
  FieldPtr   fldP;

  user.sys.n.mapID = 76;
  switch (_startup) {
  case ZAMA_CODE_ISAKO: user.sys.n.mapID = 84; break;
  default: break;
  }

  formP = FrmGetActiveForm();
  btnP  = GetObjectPtr(ZAMA_BUTTON_START);
  fldP  = GetObjectPtr(ZAMA_COMMAND_FIELD);
  chkP  = GetObjectPtr(ZAMA_CONTINUE);

  FldEraseField(fldP);
  FldSetUsable(fldP, false);
  CtlSetUsable(btnP, true);
  CtlShowControl(btnP);
  CtlSetUsable(chkP, prefs._suspended);
  CtlSetValue(chkP, prefs._suspended);
  if (prefs._suspended) {
    CtlShowControl(chkP);
  }

  FrmDrawForm(formP);
  CleanMessageField();
  LoadMapData(user.sys.n.mapID);
  DrawMap(false);
  PutMessageToField("ハイハイスクール・アドベンチャー", 52);
  PutMessageToField("   Copyright (c) 1985 ZOBplus   ", 52);
  PutMessageToField(" Copyright (c) 2002 ZOBplus hiro", 52);
  _startup = 0; /* reset */
  _over = true; /* not yet started */
}

static
void
GameOver(void)
{
  FormPtr    formP;
  ControlPtr btnP, chkP;
  FieldPtr   fldP;

  formP = FrmGetActiveForm();
  btnP  = GetObjectPtr(ZAMA_BUTTON_START);
  fldP  = GetObjectPtr(ZAMA_COMMAND_FIELD);
  chkP  = GetObjectPtr(ZAMA_CONTINUE);

  prefs._suspended = false;

  FldEraseField(fldP);
  FldSetUsable(fldP, false);
  CtlSetUsable(btnP, true);
  CtlShowControl(btnP);
  CtlSetUsable(chkP, false);
  CtlSetValue(chkP, false);

  _over = true;
}

inline UInt8
getCond(UInt16 type_, UInt16 id_, UInt16 offset_)
{
  UInt8 value;
  switch (type_) {
  case 0: value = 0; break;
  case 1: value = user.fact[id_]; break;
  case 2: value = user.place[id_]; break;
  case 3: value = user.sys.ary[id_]; break;
  case 4: value = user.vector[offset_ - 1][id_]; break;
  case 5:
  case 6:
  case 7:
  default: value = 0; break;
  }
  return value;
}

inline void
setStat(UInt16 type_, UInt16 id_, UInt16 offset_, UInt8 value_)
{
  switch (type_) {
  case 0: break;
  case 1: user.fact[id_]   = value_; break;
  case 2: user.place[id_]  = value_; break;
  case 3: user.sys.ary[id_] = value_; break;
  case 4: user.vector[offset_ - 1][id_] = value_; break;
  case 5:
  case 6:
  case 7:
  default: break;
  }
}

static Boolean SexHandleEvent(EventPtr);
static Boolean FileHandleEvent(EventPtr);
static Boolean InvHandleEvent(EventPtr);
static Boolean CutHandleEvent(EventPtr);

static
Boolean
DialogHandler(UInt16 id_)
{
  FormPtr       formP;
  UInt16        ret;
  Boolean       redraw = false;
  ControlPtr    chkP;
  Coord         x, y;
  RectangleType bounds;
  WinHandle     hWin;

  do {
    formP = FrmInitForm(id_);
    if (_silkr) {
      if (_silkVer < 0x02) {
	SilkLibDisableResize(_silkr);
      } else {
	VskSetState(_silkr, vskStateEnable, 0);
      }
      WinGetDisplayExtent(&x, &y);
      if (y > 160) {
	hWin = FrmGetWindowHandle(formP);
	WinGetBounds(hWin, &bounds);
	bounds.topLeft.y += stdSilkHeight;
	WinSetBounds(hWin, &bounds);
      }
    }
    switch(id_) {
    case ZAMA_SEX_FORM:
       FrmSetEventHandler(formP, SexHandleEvent);
      break;
    case ZAMA_FILE_FORM:
      FrmSetEventHandler(formP, FileHandleEvent);
      if (prefs._lastSaved) {
	ret  = FrmGetObjectIndex(formP, ZAMA_FILE_ID1 - 1 + prefs._lastSaved);
	chkP = FrmGetObjectPtr(formP, ret);
	CtlSetValue(chkP, true);
      }
      FrmDrawForm(formP);
      break;
    case ZAMA_INV_FORM:
      FrmSetEventHandler(formP, InvHandleEvent);
      FrmEraseForm(formP);
      for (ret = 0 ; ret < ZAMA_ITEM_NUMBERS ; ret++) {
	if (user.place[ret] == 0xff) {
	  FrmShowObject(formP, FrmGetObjectIndex(formP, ZAMA_INV_BASE + ret));
	} else {
	  FrmHideObject(formP, FrmGetObjectIndex(formP, ZAMA_INV_BASE + ret));
	}
      }
      FrmDrawForm(formP);
      break;
    case ZAMA_CUT_FORM:
      FrmSetEventHandler(formP, CutHandleEvent);
      break;
    }
    user.sys.n.dlogOk = false;
    user.sys.n.dmesg  = 0;
    ret = FrmDoDialog(formP);
    if (user.sys.n.dmesg) {
      CleanMessageField();
      DrawMessage(user.sys.n.dmesg, 0, 0);
    }
    FrmDeleteForm(formP);
    if (_silkr) {
      if (_silkVer < 0x02) {
	SilkLibEnableResize(_silkr);
      } else {
	VskSetState(_silkr, vskStateEnable, 1);
      }
    }
  } while (!user.sys.n.dlogOk);
  if (user.sys.n.dmesg == 0) {
    CleanMessageField();
  }
  switch (id_) {
  case ZAMA_SEX_FORM:
    user.sys.n.mapID = 3; /* enter to the room */
    redraw = true;
    break;
  case ZAMA_FILE_FORM:
    if (user.sys.n.dret) {
      if (user.sys.n.cmdN == 0xf) {
	SaveGame(user.sys.n.dret);
      } else {
	redraw = LoadGame(user.sys.n.dret);
	if (redraw) {
#ifdef USE_GRAPH2
	  GPaletteChange(gColorInvalid); /* initialize palette */
#else /* USE_GRAPH2 */
	  ZamaPaletteChange(InvalidColor); /* initialize palette */
#endif /* USE_GRAPH2 */
	}
      }
    }
    break;
  case ZAMA_INV_FORM: /* restore message field */
#ifdef USE_GRAPH2
    if (tsPatch) {
      FldDrawField(GetObjectPtr(ZAMA_MESSAGE_FIELD));
    } else {
      GFldDrawField(GetObjectPtr(ZAMA_MESSAGE_FIELD));
    }
#else /* USE_GRAPH2 */
    ZamaFldDrawField(GetObjectPtr(ZAMA_MESSAGE_FIELD));
#endif /* USE_GRAPH2 */
    break;
  case ZAMA_CUT_FORM: /* cut red or yellow */
    if (user.sys.n.dret == 0 || user.place[11] != 0xff) { /* fail */
      if (user.place[11] != 0xff) { /* no pinces */
	DrawMessage(0xe0, 0, 0);
      }
      /* color change to red */
#ifdef USE_GRAPH2
      GPaletteChange(gColorRed);
#else /* USE_GRAPH2 */
      ZamaPaletteChange(Red);
#endif /* USE_GRAPH2 */
      DrawMessage(0xc7, 0, 0);
      GameOver();
    } else {
      /* correctly choosen */
      user.place[11] = 0;
      user.sys.n.mapID = 74;
      ZamaSndPlay(3);
      redraw = true;
    }
    break;
  }
  return redraw;
}

static
Boolean
DarknessCheck(void)
{
  Boolean redraw = false;
  if (user.sys.n.mapID != 84 && user.sys.n.mapID != 85) {
    if ((user.sys.n.mapID >= 47 && user.sys.n.mapID <= 49)||
	(user.sys.n.mapID >= 67 && user.sys.n.mapID <= 71)||
	 user.sys.n.mapID == 61 || user.sys.n.mapID == 64 ||
	 user.sys.n.mapID == 75 || user.sys.n.mapID == 65 ||
	 user.sys.n.mapID == 74 || user.sys.n.mapID == 77) {
      if (user.fact[7] != 0) {
	if (user.fact[6] != 0) {
	  /* semi-blue out */
#ifdef USE_GRAPH2
	  GPaletteChange(gColorBlue);
#else /* USE_GRAPH2 */
	  ZamaPaletteChange(Blue);
#endif /* USE_GRAPH2 */
	}
      } else {
	user.sys.n.mapW  = user.sys.n.mapID;
	user.sys.n.mapID = 84;
	redraw = true;
      }
    } else {
      if (user.fact[6] != 0) {
	/* normal state */
#ifdef USE_GRAPH2
	GPaletteChange(gColorInvalid);
#else /* USE_GRAPH2 */
	ZamaPaletteChange(InvalidColor);
#endif /* USE_GRAPH2 */
      }
    }
  }
  return redraw;
}
  
static
Boolean
RuleInterpreter(UInt16 cmdN_, UInt16 objN_)
{
  Boolean    res, ok, redraw;
  UInt16     i;
  RulePtr    rule;
  CmdBlkPtr  blk;
  UInt16     v1, v2;
  DmOpenRef  dbP;
  MemHandle  rec;

  res = false;
  if ((dbP = OpenDatabase("ZAMArule")) == NULL) {
    /* error */
    FrmCustomAlert(ZAMA_FATAL_ALERT, "ルール", NULL, NULL);
    return false;
  }
  if ((rec = DmQueryRecord(dbP, 0)) == NULL) {
    FrmCustomAlert(ZAMA_FATAL_ALERT, "ルール", NULL, NULL);
    goto _done;
  }
  rule = (RulePtr)MemHandleLock(rec);

  for (i = 0 ; rule[i]._mapId != EndOfRule ; i++) {
    if (rule[i]._mapId && rule[i]._mapId != user.sys.n.mapID) continue;
    if (rule[i]._cmdN  && rule[i]._cmdN  != cmdN_)            continue;
    if (rule[i]._objN  && rule[i]._objN  != objN_)            continue;
    /* check condition block */
    ok = true;
    for (blk = (CmdBlkPtr)rule[i]._args ; blk->_head._act == ActComp ; blk++) {
      v1 = getCond(blk->_head._type, blk->_head._id, 
		   (blk->_body._type << 5) | blk->_body._id);
      if (blk->_head._type == TYPE_SYSTEM && blk->_head._id == 5) {
	user.sys.n.rand = SysRandom(0) % 256; /* update random number */
      }
      v2 = blk->_body._value;
      if (blk->_body._type && blk->_head._type != TYPE_VECTOR) {
	v2 = getCond(blk->_body._type, blk->_body._id, 0);
	if (blk->_body._type == TYPE_SYSTEM && blk->_body._id == 5) {
	  user.sys.n.rand = SysRandom(0) % 256; /* update random number */
	}
      }
      switch (blk->_head._op) {
      case CMP_EQ: ok = ok && (v1 == v2); break;
      case CMP_NE: ok = ok && (v1 != v2); break;
      case CMP_GT: ok = ok && (v1 >  v2); break;
      case CMP_GE: ok = ok && (v1 >= v2); break;
      case CMP_LT: ok = ok && (v1 <  v2); break;
      case CMP_LE: ok = ok && (v1 <= v2); break;
      default:
	ok = false; /* no defalut operation is defined */
      }
      if (!ok) break; /* short circuit */
    }
    if (ok) break;
  }
  if (ok) {
    /* do action */
    redraw = false;
    ok  = false;
    for ( ; blk->_head._op ; blk++) {
      switch (blk->_head._op) {
      case ACT_MOVE:
	if (user.vector[user.sys.n.mapID - 1][blk->_body._value]) {
	  /* moveable */
	  user.sys.n.mapID =
	    user.vector[user.sys.n.mapID - 1][blk->_body._value];
	  redraw = true;
	  res = true;
	} else {
	  /* teacher check */
	  if (user.fact[1] == user.sys.n.mapID && user.sys.n.rand > 85) {
	    /* teacher catches you */
	    DrawMessage(0xb5, 0, 0);
	    /* game over */
	    user.fact[1] = 0; /* teacher gone */
	    GameOver();
	    res = true;
	  } else {
	    /* you cannot move */
	    DrawMessage(0xb6, 0, 0);
	  }
	  user.sys.n.rand = SysRandom(0) % 256;
	  res = true;
	}
	break;
      case ACT_ASGN:
	v1 = (blk->_body._type << 5) | blk->_body._id;
	v2 = blk->_body._value;
	if (blk->_body._type && blk->_head._type != TYPE_VECTOR) {
	  v2 = getCond(blk->_body._type, blk->_body._id, 0);
	}
	setStat(blk->_head._type, blk->_head._id, v1, v2);
	if (blk->_head._type == TYPE_PLACE || blk->_head._type == TYPE_FACT) {
	  if (blk->_head._type == TYPE_PLACE) {
	    if (v2 == 0xff) {
	      redraw = true;
	    } else {
	      ObjectCheck(user.sys.n.mapID, true);
	    }
	  }
	  ok = true;
	} else if(blk->_head._type == TYPE_SYSTEM) {
	  if (blk->_head._id == 0) {
	    redraw = true;
	  } else if (blk->_head._id == 5) {
	    user.sys.n.rand = SysRandom(0) % 256; /* update random number */
	  }
	}
	res = true;
	break;
      case ACT_MESG:
	DrawMessage(blk->_body._value, 0, 0);
	res = true;
	break;
      case ACT_DLOG:
	redraw = DialogHandler(ZAMA_DIALOG_BASE + blk->_body._value);
	res = true;
	break;
      case ACT_LOOK:
	if (blk->_body._value == 0) { /* look back */
	  user.sys.n.mapID = user.sys.n.mapW;
	  user.sys.n.mapW  = 0;
	} else {
	  user.sys.n.mapW  = user.sys.n.mapID;
	  user.sys.n.mapID = blk->_body._value;
	}
	redraw = true;
	res = true;
	break;
      case ACT_SND:
	ZamaSndPlay(blk->_body._value);
	break;
      case ACT_OVER:
	ok = false;
	switch(blk->_body._value) {
	case 0: /* simply put dialog */
	  user.fact[1] = 0; /* teacher gone */
	  PutMessageToField("ＧａｍｅＯｖｅｒ", 16);
	  break;
	case 1: /* screen to red */
	  PutMessageToField("ＧａｍｅＯｖｅｒ", 16);
#ifdef USE_GRAPH2
	  GPaletteChange(gColorRed);
#else /* USE_GRAPH2 */
	  ZamaPaletteChange(Red);
#endif /* USE_GRAPH2 */
	  break;
	case 2: /* game clear */
	  PutMessageToField("Ｆｉｎｉｓｈ", 12);
#ifdef USE_GRAPH2
	  GPaletteChange(gColorInvalid);
#else /* USE_GRAPH2 */
	  ZamaPaletteChange(InvalidColor);
#endif /* USE_GRAPH2 */
	  FrmHelp(ZAMA_CREDIT_STR); /* put credit */
	  break;
	}
	GameOver();
	res = true;
	break;
      default:
	/* error */
	res = false;
	break;
      }
    }
    if (ok) {
      PutMessageToField("Ｏ.Ｋ.", 6);
    }
    if (user.sys.n.mapID == 74) {
      switch (++user.fact[13]) {
      case 4:  	DrawMessage(0xe2, 0, 0); break;
      case 6:  	DrawMessage(0xe3, 0, 0); break;
      case 10:  DrawMessage(0xe4, 0, 0); break;
      default:  break;
      }
    }
    redraw = DarknessCheck() || redraw;
    if (redraw) {
      LoadMapData(user.sys.n.mapID);
      DrawMap(true);
      ObjectCheck(user.sys.n.mapID, true);
    }
  }
  MemHandleUnlock(rec);
 _done:;
  DmCloseDatabase(dbP);
  return res;
}

static
Boolean
CommandLoop(void)
{
  MemHandle     hStr;
  Char         *strP;
  UInt16        end, rd;
  FieldPtr      fldP;

  if ((hStr = FldGetTextHandle(GetObjectPtr(ZAMA_COMMAND_FIELD))) == NULL) {
    /* fail to get string */
    return false;
  }
  strP = (Char*)MemHandleLock(hStr);
  end  = StrLen(strP);
  VOSplit(strP);
  CleanMessageField();

  user.sys.n.cmdN = SearchWord(cmd, true);
  user.sys.n.objN = SearchWord(obj, false);
  user.sys.n.rand = SysRandom(0) % 256;

  /* status check */
  if (user.fact[5] == 2) {
    user.vector[ 9 - 1][6] = 4;
    user.vector[10 - 1][6] = 19;
    user.vector[11 - 1][6] = 6;
    user.vector[14 - 1][6] = 7;
    user.vector[16 - 1][6] = 17;
    user.vector[24 - 1][6] = 8;
    user.vector[25 - 1][6] = 20;
    user.vector[26 - 1][6] = 22;
    user.vector[28 - 1][6] = 21;
    user.vector[30 - 1][6] = 5;
    user.vector[37 - 1][6] = 32;
    user.vector[38 - 1][6] = 33;
    user.vector[39 - 1][6] = 34;
    user.vector[40 - 1][6] = 31;
    user.vector[42 - 1][6] = 35;
    user.vector[44 - 1][6] = 36;
    user.fact[5] = 1;
  }
  if (user.fact[3] > 0 && user.fact[7] == 1) { /* light on */
    --user.fact[3]; /* battery */
    if (user.fact[3] < 8 && user.fact[3] > 0) {
      user.fact[6] = 1;
      DrawMessage(0xd9, 0, 0);
    }
    if (user.fact[3] == 0) { /* battery wear out */
      user.fact[7] = 0;
      DrawMessage(0xc0, 0, 0);
    }
  }
  if (user.fact[11] > 0) { /* count down */
    --user.fact[11];
    if (user.fact[11] == 0) {
      ZamaSndPlay(2);
      DrawMessage(0xd8, 0, 0);
      if (user.place[7] == 48) {
	user.vector[75 - 1][0] = 77;
	user.vector[68 - 1][2] = 77;
	DrawMessage(0xda, 0, 0);
      }
      if (user.place[7] == 255 || user.place[7] == user.sys.n.mapID) {
	/* explosion within the room where you are */
	/* change screen to red */
#ifdef USE_GRAPH2
	GPaletteChange(gColorRed);
#else /* USE_GRAPH2 */
	ZamaPaletteChange(Red);
#endif /* USE_GRAPH2 */
	DrawMessage(0xcf, 0, 0);
	DrawMessage(0xcb, 0, 0);
	/* game over */
	GameOver();
	goto _done;
      } else {
	user.place[7] = 0; /* explosion ... lose bomber */
      }
    }
  }

  if (user.sys.n.cmdN == ZAMA_CMD_INVALID || user.sys.n.cmdN >= ZAMA_CMD_END) {
    DrawMessage(0xaf + user.sys.n.rand % 5, 0, 0);
    goto _done;
  }
  if (! RuleInterpreter(user.sys.n.cmdN, user.sys.n.objN)) {
    DrawMessage(0, user.sys.n.cmdN, user.sys.n.objN);
  }

  /* teacher check */
  if (!_over) {
    rd = 100 + user.sys.n.mapID + ((user.fact[1] > 0) ? 1000 : 0);
    if (rd < SysRandom(0) % 3000) {
      if (user.fact[1] == user.sys.n.mapID) {
	DrawMap(false); /* erase teacher */
      }
      user.fact[1] = 0;
    } else {
      user.fact[1] = user.sys.n.mapID; /* appear teacher */
    }
  }
  if ((user.sys.n.mapID >= 50 && user.sys.n.mapID <= 53) ||
      (user.sys.n.mapID >= 64 && user.sys.n.mapID <= 77) ||
       user.sys.n.mapID == 83 || user.sys.n.mapID == 48 ||
       user.sys.n.mapID == 61 || user.sys.n.mapID == 1 ||
       user.sys.n.mapID == 86) {
    user.fact[1] = 0;
  }
  if (user.fact[1] == user.sys.n.mapID) {
    DrawTeacher();
    ZamaSndPlay(6);
    DrawMessage(0xb4, 0, 0);
  }
 _done:;
  MemHandleUnlock(hStr);
  fldP = GetObjectPtr(ZAMA_COMMAND_FIELD);
  FldDelete(fldP, 0, end);
  FldDrawField(fldP);
  return true;
}

static
Boolean
MainMenuEvent(EventPtr eventP)
{
  Boolean       handled = false;
  FormPtr       dlogP;
  FontID        id;
  UInt16        lines, plane, sound;
  Coord         x, y;
  FieldPtr      fldP;

  fldP = GetObjectPtr(ZAMA_COMMAND_FIELD);
  switch (eventP->data.menu.itemID) {
  case ZAMA_FONT_MENU:
    /* menu select */
#ifdef USE_GRAPH2
    id = ZamaFontSelect(prefs._curFont);
#else /* USE_GRAPH2 */
    id = ZamaFontSelect(prefs._curFont);
#endif /* USE_GRAPH2 */
    if (prefs._curFont != id) {
      prefs._curFont = id;
      fldP = GetObjectPtr(ZAMA_MESSAGE_FIELD);
      FldSetFont(fldP, id);
#ifdef USE_GRAPH2
      if (tsPatch) {
	FldDrawField(fldP);
      } else {
	GFldDrawField(fldP);
      }
#else /* USE_GRAPH2 */
      ZamaFldDrawField(fldP);
#endif /* USE_GRAPH2 */
      UpdateMessageFieldScrollbar();
    }
    handled = true;
    break;
  case ZAMA_PREF_MENU:
    /* pop-up prefs dialog */
    dlogP = FrmInitForm(ZAMA_PREF_FORM);
    lines = FrmGetObjectIndex(dlogP, ZAMA_DRAW_LINE);
    plane = FrmGetObjectIndex(dlogP, ZAMA_DRAW_PLANE);
    /* sound */
    sound = FrmGetObjectIndex(dlogP, ZAMA_SND_SWITCH);
    if (_silkr){
      WinGetDisplayExtent(&x, &y);
      if (y > 160) {
	RectangleType bounds;
	WinHandle     hWin;
	hWin = FrmGetWindowHandle(dlogP);
	WinGetBounds(hWin, &bounds);
	bounds.topLeft.y += stdSilkHeight;
	WinSetBounds(hWin, &bounds);
      }
    }
#ifndef USE_GRAPH2
    if (!ZamaHR()) {
      /* not support line draw mode */
      CtlSetUsable(FrmGetObjectPtr(dlogP, lines), false);
      prefs._linedraw = false;
    }
#endif /* USE_GRAPH2 */
    CtlSetValue(FrmGetObjectPtr(dlogP, lines), prefs._linedraw);
    CtlSetValue(FrmGetObjectPtr(dlogP, plane), !prefs._linedraw);
    CtlSetValue(FrmGetObjectPtr(dlogP, sound), prefs._sound);
    if (FrmDoDialog(dlogP) == ZAMA_BUTTON_OK) {
      /* change status */
#ifndef USE_GRAPH2
      if (ZamaHR()) {
#endif /* USE_GRAPH2 */
	prefs._linedraw =
	    (FrmGetControlGroupSelection(dlogP, ZAMA_DRAW_STYLE) == lines);
#ifndef USE_GRAPH2
      }
#endif /* USE_GRAPH2 */
      prefs._sound = CtlGetValue(FrmGetObjectPtr(dlogP, sound));
    }
    FrmDeleteForm(dlogP);
    handled = true;
    break;
  case ZAMA_HELP_CMD:
    FrmHelp(ZAMA_HELP_STR);
    break;
  case ZAMA_ABOUT_CMD:
    dlogP = FrmInitForm(ZAMA_FRM_ABOUT);
    FrmDoDialog(dlogP);
    FrmDeleteForm(dlogP);
    break;
  }
  return handled;
}

static
Boolean
MainHeightChanged(EventPtr eventP)
{
  RectangleType bounds;
  FormPtr       formP;
  WinHandle     hWin;
  Int16         dy;
  Coord         nx, ny;
  UInt16        i, idx;
  FieldPtr      fldP;
  Boolean       res = true;
  UInt16        objs[] = {
    ZAMA_COMMAND_LABEL,
    ZAMA_BUTTON_START,
    ZAMA_CONTINUE,
    0
  };

  formP = FrmGetActiveForm();
  hWin  = FrmGetWindowHandle(formP);
  WinGetBounds(hWin, &bounds);
  WinGetDisplayExtent(&nx, &ny);
  if (ny == bounds.topLeft.y + bounds.extent.y) {
    /* size has not changed */
    return res;
  }
  bounds.extent.y = ny - bounds.topLeft.y;
  bounds.extent.x = nx - bounds.topLeft.x;
  FrmEraseForm(formP);
  WinSetBounds(hWin, &bounds);
  /* relocate object */
  dy = (ny == 160) ? -stdSilkHeight : stdSilkHeight;
  /* GSI */
  for (i = 0 ; i < FrmGetNumberOfObjects(formP) ; i++) {
    if (FrmGetObjectType(formP, i) == frmGraffitiStateObj) {
      /* found GSI */
      FrmGetObjectPosition(formP, i, &nx, &ny);
      ny += dy;
      FrmSetObjectPosition(formP, i, nx, ny);
      break; /* needless to search any more */
    }
  }
  for (i = 0 ; objs[i] ; i++) {
    idx = FrmGetObjectIndex(formP, objs[i]);
    FrmGetObjectBounds(formP, idx, &bounds);
    bounds.topLeft.y += dy;
    FrmSetObjectBounds(formP, idx, &bounds);
  }
  /* move command field */
  fldP = FrmGetObjectPtr(formP, FrmGetObjectIndex(formP, ZAMA_COMMAND_FIELD));
  FldGetBounds(fldP, &bounds);
  bounds.topLeft.y += dy;
  FldSetBounds(fldP, &bounds);
  FrmSetFocus(formP, FrmGetObjectIndex(formP, ZAMA_COMMAND_FIELD));
  FldDrawField(fldP);
  /* extent message field */
  fldP = FrmGetObjectPtr(formP, FrmGetObjectIndex(formP, ZAMA_MESSAGE_FIELD));
  FldGetBounds(fldP, &bounds);
  bounds.extent.y += dy * 2;
  FldSetBounds(fldP, &bounds);
  /* update scrollbar */
  idx = FrmGetObjectIndex(formP, ZAMA_SCROLL_BAR);
  FrmGetObjectBounds(formP, idx, &bounds);
  bounds.extent.y += dy;
  FrmSetObjectBounds(formP, idx, &bounds);
  /* update */
  UpdateMessageFieldScrollbar();
  FrmDrawForm(formP);
#ifdef USE_GRAPH2
  if (tsPatch) {
    FldDrawField(fldP);
  } else {
    GFldDrawField(fldP);
  }
#else /* USE_GRAPH2 */
  ZamaFldDrawField(fldP);
#endif /* USE_GRAPH2 */
  DrawMap(false);
  ObjectCheck(user.sys.n.mapID, false);

  return res;
}

static
Boolean
MainHandleEvent(EventPtr eventP)
{
  Boolean       handled = false;
  FormPtr       formP;
  FieldPtr      fldP;
  ControlPtr    btnP, chkP;
  UInt16        lines;
      
  formP = FrmGetActiveForm();
  /* resize check */
  if (_silkr && _resized && MainHeightChanged(eventP)) {
    _resized = false;
  }
  if (eventP->eType == frmOpenEvent) {
    GameInit();
    handled = true;
  } else if (eventP->eType == frmUpdateEvent) {
    FrmDrawForm(formP);
#ifdef USE_GRAPH2
    if (tsPatch) {
      FldDrawField(GetObjectPtr(ZAMA_MESSAGE_FIELD));
    } else {
      GFldDrawField(GetObjectPtr(ZAMA_MESSAGE_FIELD));
    }
#else /* USE_GRAPH2 */
    ZamaFldDrawField(GetObjectPtr(ZAMA_MESSAGE_FIELD));
#endif /* USE_GRAPH2 */
    DrawMap(false); /* maybe re-draw map */
    ObjectCheck(user.sys.n.mapID, true);
    FrmSetFocus(formP, FrmGetObjectIndex(formP, ZAMA_COMMAND_FIELD));
    handled = true;
  } else if (eventP->eType == ctlSelectEvent) {
    switch (eventP->data.ctlSelect.controlID) {
    case ZAMA_BUTTON_START:
      /* enable command field and disable start button */
      btnP = GetObjectPtr(ZAMA_BUTTON_START);
      fldP = GetObjectPtr(ZAMA_COMMAND_FIELD);
      chkP = GetObjectPtr(ZAMA_CONTINUE);
      FrmEraseForm(formP);
      CleanMessageField();
      CtlSetUsable(btnP, false);
      CtlSetUsable(chkP, false);
      FldSetUsable(fldP, true);
      /* init game */
#ifdef USE_GRAPH2
      GPaletteChange(gColorInvalid); /* initialize palette */
#else /* USE_GRAPH2 */
      ZamaPaletteChange(InvalidColor); /* initialize palette */
#endif /* USE_GRAPH2 */
      if (CtlGetValue(chkP)) { /* load */
	LoadGame(0);
	DarknessCheck();    /* need to check of darkness */
      } else {
	LoadInitData();
	user.sys.n.mapID = 1; /* set map to start point */
	SaveGame(0);        /* clear suspended data */
      }
      _over = false; /* start game */
      FldDrawField(fldP);
      FrmDrawForm(formP);
      FrmSetFocus(formP, FrmGetObjectIndex(formP, ZAMA_COMMAND_FIELD));
      CleanMessageField();
      LoadMapData(user.sys.n.mapID);
      DrawMap(true);
      ObjectCheck(user.sys.n.mapID, true);
      if (user.fact[1] == user.sys.n.mapID) { /* teacher check */
	DrawTeacher();
	DrawMessage(0xb4, 0, 0);
      }
      prefs._suspended = true; /* continuable */
      handled = true;
      break;
    }
  } else if (eventP->eType == keyDownEvent) {
    fldP  = GetObjectPtr(ZAMA_MESSAGE_FIELD);
    lines = FldGetVisibleLines(fldP) - 1;
    switch (eventP->data.keyDown.chr) {
    case pageUpChr:
      if (FldScrollable(fldP, winUp)) {
	FldScrollField(fldP, lines, winUp);
	UpdateMessageFieldScrollbar();
      }
      handled = true;
      break;
    case pageDownChr:
      if (FldScrollable(fldP, winDown)) {
	FldScrollField(fldP, lines, winDown);
	UpdateMessageFieldScrollbar();
      }
      handled = true;
      break;
    }
  } else if (eventP->eType == fldChangedEvent) {
    /* update scrollbar */
    if (eventP->data.fldChanged.fieldID == ZAMA_MESSAGE_FIELD) {
      UpdateMessageFieldScrollbar();
      handled = true;
    }
  } else if (eventP->eType == sclRepeatEvent || eventP->eType == sclExitEvent){
    ScrollMessageField();
    handled = true;
  } else if (eventP->eType == menuOpenEvent) {
    handled = true;
  } else if (eventP->eType == menuEvent) {
    handled = MainMenuEvent(eventP);
  } else if (eventP->eType == appStopEvent) {
    FrmEraseForm(formP);
    FrmDeleteForm(formP);
    handled = true;
  }
  return handled;
}

static
Boolean
SexHandleEvent(EventPtr eventP)
{
  UInt16  sid;
  Boolean handled = false;
  FormPtr formP   = FrmGetActiveForm();
  
  if (eventP->eType == ctlSelectEvent) {
    switch (eventP->data.ctlSelect.controlID) {
    case ZAMA_BUTTON_OK:
      sid = FrmGetControlGroupSelection(formP, ZAMA_SEX_GROUP);
      if (sid == FrmGetObjectIndex(formP, ZAMA_SEX_BOY)) {
	user.sys.n.dret   = user.fact[0] = 1;
	user.sys.n.dlogOk = true;
	user.sys.n.dmesg  = 0;
      } else if (sid == FrmGetObjectIndex(formP, ZAMA_SEX_GIRL)) {
	user.sys.n.dret   = user.fact[0] = 2;
	user.sys.n.dlogOk = true;
	user.sys.n.dmesg = 0;
      } else {
	user.sys.n.dret   = 0;
	user.sys.n.dlogOk = false;
	user.sys.n.dmesg  = 0xbc;
      }
      break;
    }
  }
  return handled;
}

static
Boolean
FileHandleEvent(EventPtr eventP)
{
  Boolean handled = false;
  UInt16  fid;
  FormPtr formP = FrmGetActiveForm();

  if (eventP->eType == ctlSelectEvent) {
    switch (eventP->data.ctlSelect.controlID) {
    case ZAMA_BUTTON_OK:
      fid = FrmGetControlGroupSelection(formP, ZAMA_FILE_GROUP);
      if (fid == FrmGetObjectIndex(formP, ZAMA_FILE_ID1)) {
	user.sys.n.dret = 1;
      } else if (fid == FrmGetObjectIndex(formP, ZAMA_FILE_ID2)) {
	user.sys.n.dret = 2;
      } else if (fid == FrmGetObjectIndex(formP, ZAMA_FILE_ID3)) {
	user.sys.n.dret = 3;
      } else {
	user.sys.n.dret = 0;
      }
      user.sys.n.dlogOk = true;
      break;
    }
  }
  return handled;
}

static
Boolean
InvHandleEvent(EventPtr eventP)
{
  Boolean handled = false;

  if (eventP->eType == ctlSelectEvent) {
    switch (eventP->data.ctlSelect.controlID) {
    case ZAMA_BUTTON_OK:
      user.sys.n.dlogOk = true;
      break;
    }
  }
  return handled;
}

static
Boolean
CutHandleEvent(EventPtr eventP)
{
  UInt16  color;
  Boolean handled = false;
  FormPtr formP = FrmGetActiveForm();

  if (eventP->eType == ctlSelectEvent) {
    switch (eventP->data.ctlSelect.controlID) {
    case ZAMA_BUTTON_OK:
      user.sys.n.dlogOk = true; /* no time kidding */
      color = FrmGetControlGroupSelection(formP, ZAMA_CUT_GROUP);
      if (color == FrmGetObjectIndex(formP, ZAMA_CUT_RED)) {
	user.sys.n.dret = 1;
      } else {
	user.sys.n.dret = 0;
      }
      break;
    }
  }
  return handled;
}

static
Boolean
MainKeyHandleEvent(EventPtr eventP, UInt32 keys)
{
  Boolean handled = false;
  EventType event;

  if (FrmGetWindowHandle(FrmGetActiveForm()) != WinGetActiveWindow()) {
    /* maybe menu or another widget popping up */
    return false;
  }

  if (eventP->eType == nilEvent) { /* Game PAD */
    if (keys & (keyBitPageUp | keyBitPageDown | keyBitHard1 | keyBitHard2)) {
      /* move direction */
      char *dir = NULL;
      if (keys & keyBitPageUp) {
	dir = "n";
      } else if (keys & keyBitPageDown) {
	dir = "s";
      } else if (keys & keyBitHard1) {
	dir = "w";
      } else if (keys & keyBitHard2) {
	dir = "e";
      }
      if (dir) {
	FldInsert(GetObjectPtr(ZAMA_COMMAND_FIELD), dir, StrLen(dir));
	event.eType = keyDownEvent;
	event.data.keyDown.chr = chrLineFeed;
	EvtAddEventToQueue(&event);
	handled = true;
      }
    }
  }

  return handled;
}

static
Boolean
KeyHandleEvent(EventPtr eventP)
{
  UInt32       keys;
  Boolean      handled = false;

  if ((keys = KeyCurrentState()) == 0) { /* no key has been pressed */
    return false;
  }
  switch (FrmGetActiveFormID()) {
  case ZAMA_MAIN_FORM:
    handled = MainKeyHandleEvent(eventP, keys);
    break;
  default:
    handled = false;
    break;
  }
  if (!handled) {
    EventType event;
    MemSet(&event, 0, sizeof(EventType));
    if (eventP->eType == nilEvent && keys & (keyBitHard3 | keyBitHard4)) {
      event.eType = keyDownEvent;
      event.data.keyDown.chr =(keys & keyBitHard3) ? vchrJogPush : vchrJogBack;
      EvtAddEventToQueue(&event);
      handled = true;
    } else if (eventP->eType == nilEvent &&
	       keys & (keyBitPageUp | keyBitPageDown)) {
      event.eType = keyDownEvent;
      event.data.keyDown.chr = (keys & keyBitPageUp) ? vchrJogUp : vchrJogDown;
      EvtAddEventToQueue(&event);
      handled = true;
    }
  }
  _ignoreKey = handled;
  return handled;
}

static
Boolean
AppHandleEvent(EventPtr eventP)
{
  UInt16  formId;
  FormPtr formP;

  Boolean handled = false;
  if (eventP->eType == frmLoadEvent) {
    formId = eventP->data.frmLoad.formID;
    formP  = FrmInitForm(formId);
    FrmSetActiveForm(formP);
    FldSetFont(GetObjectPtr(ZAMA_MESSAGE_FIELD), prefs._curFont);
    switch (formId) {
    case ZAMA_MAIN_FORM:
      FrmSetEventHandler(formP, MainHandleEvent);
#ifdef USE_GRAPH2
      if (!tsPatch) {
	GFldModifyField(GetObjectPtr(ZAMA_MESSAGE_FIELD));
      }
#else /* USE_GRAPH2 */
      ZamaFldModifyField(GetObjectPtr(ZAMA_MESSAGE_FIELD));
#endif /* USE_GRAPH2 */
      handled = true;
      break;
    default:
      break;
    }
  }
  return handled;
}

void
EventLoop(void)
{
  EventType event;
  UInt16    err;
  UInt16    formID;

  _ignoreKey = false;
  do {
    formID = FrmGetActiveFormID();
    EvtGetEvent(&event, evtWaitForever);
    if (!_ignoreKey && KeyHandleEvent(&event)) {
      continue;
    }
    _ignoreKey = false;
    if (event.eType == keyDownEvent && event.data.keyDown.chr == chrLineFeed) {
      /* accept command */
      if (formID == ZAMA_MAIN_FORM) {
	CommandLoop();
	continue;
      }
    }
    //    if (_silkr && event.eType == menuEvent) SilkLibDisableResize(_silkr);
    if (!SysHandleEvent(&event)) {
      if (!MenuHandleEvent(NULL, &event, &err)) {
#if 0
	if (event.eType == menuCmdBarOpenEvent) {
	  /* command stroke */
	  event.data.menuCmdBarOpen.preventFieldButtons = true;
	}
#endif
	if(!AppHandleEvent(&event)) {
	  if (_silkr) {
	    if (_silkVer < 0x02) {
	      SilkLibDisableResize(_silkr);
	    } else {
	      VskSetState(_silkr, vskStateEnable, 0);
	    }
	  }
	  FrmDispatchEvent(&event);
	  if (_silkr) {
	    if (_silkVer < 0x02) {
	      SilkLibEnableResize(_silkr);
	    } else {
	      VskSetState(_silkr, vskStateEnable, 1);
	    }
	  }
	}
      }
    }
    //    if (_silkr) SilkLibEnableResize(_silkr);
  } while (event.eType != appStopEvent);
}
/* notification & silk */
static
Err
SilkHeightChange(SysNotifyParamType *paramP_)
{
  FormPtr       formP, afrmP;
  Coord         nx, ny;
  RectangleType bounds;
  Boolean      *resized;

  afrmP = FrmGetActiveForm();
  formP = FrmGetFormPtr(ZAMA_MAIN_FORM);
  WinGetBounds(FrmGetWindowHandle(formP), &bounds);
  WinGetDisplayExtent(&nx, &ny);
  if (afrmP != formP || ny == bounds.topLeft.y + bounds.extent.y) {
    /* works only for display hight change */
    if (paramP_) paramP_->handled = false;
    return errNone;
  }
  _resized = true; /* notify size has changed */
  GrfCleanState();
  GrfInitState();
  GsiInitialize();
  GsiSetShiftState(0, gsiShiftNone);
  if (paramP_) {
    resized = paramP_->userDataP;
    if (resized) *resized = true;
  }
  if (paramP_) paramP_->handled = true;
  return errNone;
}

static
Boolean
SilkStart(void)
{
  Err                err;
  LocalID            codeResId;
  SonySysFtrSysInfoP sonySysFtrSysInfoP;
  UInt16             cardNo;

  _silkr = 0;
  if ((err = FtrGet(sonySysFtrCreator,
		    sonySysFtrNumSysInfoP,
		    (UInt32*)&sonySysFtrSysInfoP))) {
    return false;
  }
  if ((sonySysFtrSysInfoP->extn & sonySysFtrSysInfoExtnSilk) &&
      (sonySysFtrSysInfoP->libr & sonySysFtrSysInfoLibrSilk)) {
    if ((err = SysLibFind(sonySysLibNameSilk, &_silkr))) {
      if (err == sysErrLibNotFound) {
	err = SysLibLoad('libr', sonySysFileCSilkLib, &_silkr);
      }
    }
  }
  _resized = false;
  if (err || _silkr == 0) {
    _silkr = 0;
    return false;
  }
  if (SilkLibOpen(_silkr) == errNone) {
    if ((_silkVer = VskGetAPIVersion(_silkr)) < 0x02) {
      SilkLibEnableResize(_silkr);
    } else {
      VskSetState(_silkr, vskStateEnable, 1);
      VskSetState(_silkr, vskStateResize, vskResizeMax);
    }
  }

  if ((err = SysCurAppDatabase(&cardNo, &codeResId)) != errNone) {
    return false;
  }
  err = SysNotifyRegister(cardNo, codeResId, sysNotifyDisplayChangeEvent,
			  SilkHeightChange, 
			  sysNotifyNormalPriority,
			  &_resized);
  return err == errNone;
}

static
Boolean
SilkStop(void)
{
  Err     err;
  LocalID codeResId;
  UInt16  cardNo;
  Boolean res = true;

  if ((err = SysCurAppDatabase(&cardNo, &codeResId)) != errNone) {
    res = false;
  }
  if (res) {
    err = SysNotifyUnregister(cardNo, codeResId, sysNotifyDisplayChangeEvent,
			      sysNotifyNormalPriority);
  }
  if (_silkr) {
    if (_silkVer < 0x02) {
      SilkLibResizeDispWin(_silkr, silkResizeNormal);
      SilkLibDisableResize(_silkr);
    } else {
      VskSetState(_silkr, vskStateResize, vskResizeMax);
      VskSetState(_silkr, vskStateEnable, 0);
    }
    SilkLibClose(_silkr);
  }
  return res && err == errNone;
}

static
void
ZamaMapInit(void)
{
  MemSet(map.graphic,  sizeof(map.graphic),  0xff);
  MemSet(map.reaction, sizeof(map.reaction), 0);
  MemSet(map.mesg,     sizeof(map.mesg),     0);
}

static
Boolean
InitTsPatch(void)
{
  UInt32  ver;
  Boolean ok = false; 

  tinyFontID = stdFont;
  tinyBoldFontID = boldFont;
  smallFontID = largeFont;
  smallSymbolFontID = symbolFont;
  smallSymbol11FontID = symbol11Font;
  smallSymbol7FontID = symbol7Font;
  smallLedFontID = ledFont;
  smallBoldFontID = largeBoldFont;

  if (FtrGet(SmallFontAppCreator, SMF_FTR_SMALL_FONT_SUPPORT, &ver) == errNone) {
    FtrGet(SmallFontAppCreator, SMF_FTR_TINY_FONT, &tinyFontID);
    FtrGet(SmallFontAppCreator, SMF_FTR_TINY_BOLD_FONT, &tinyBoldFontID);
    FtrGet(SmallFontAppCreator, SMF_FTR_SMALL_FONT, &smallFontID);
    FtrGet(SmallFontAppCreator, SMF_FTR_SYMBOL_FONT, &smallSymbolFontID);
    FtrGet(SmallFontAppCreator, SMF_FTR_SYMBOL11_FONT, &smallSymbol11FontID);
    FtrGet(SmallFontAppCreator, SMF_FTR_SYMBOL7_FONT, &smallSymbol7FontID);
    FtrGet(SmallFontAppCreator, SMF_FTR_LED_FONT, &smallLedFontID);
    FtrGet(SmallFontAppCreator, SMF_FTR_SMALL_BOLD_FONT, &smallBoldFontID);
    ok = true;
  }
  return ok;
}

static
Boolean
StartApplication(void)
{
  UInt16  size, ver;
  Boolean res = false;

#ifdef USE_GRAPH2
  if (!GInit()) {
    return false;
  }
#else /* USE_GRAPH2 */
  if (!ZamaGraphInit()) {
    return false;
  }
#endif /* USE_GRAPH2 */
  res = true;
  RctSetRectangle(&pict, 32, 32, 256, 152);
  LoadInitData();
  size = sizeof(ZamaPrefsType);
  MemSet(&prefs, size, 0);
  prefs._linedraw = true;
  FtrGet(sysFtrCreator, sysFtrDefaultFont, (UInt32*)&prefs._curFont);
#ifndef USE_GRAPH2
  if (ZamaHR()) {
    prefs._curFont += hrStdFont;
  }
#endif /* USE_GRAPH2 */
  ZamaMapInit();
  ver = PrefGetAppPreferences(ZAMA_CREATOR_ID,
			      ZAMA_PREFS_ID,
			      &prefs, &size, true);
  if (ver != ZAMA_PREFS_VER) {
    /* upgrade ? */
  }
  SilkStart();
  tsPatch = InitTsPatch();
  return res;
}

static
Boolean
StopApplication(void)
{
#ifdef USE_GRAPH2
  GFinalize();
#else /* USE_GRAPH2 */
  ZamaGraphClean();
#endif /* USE_GRAPH2 */
  if (user.sys.n.mapID != 76) { /* not opening/clear screen */
    SaveGame(0);
  }
  PrefSetAppPreferences(ZAMA_CREATOR_ID,
			ZAMA_PREFS_ID,
			ZAMA_PREFS_VER,
			&prefs,
			sizeof(ZamaPrefsType), true);
  SilkStop();
  return true;
}

static void
ZamaSearchDrawName(const Char *strP_, Coord x_, Coord y_, UInt16 width_)
{
  Char *ptr = StrChr(strP_, linefeedChr);
  UInt16 titleLen = (ptr == NULL) ? StrLen(strP_) : (UInt16)(ptr - strP_);
  if (FntWidthToOffset(strP_, titleLen, width_, NULL, NULL) == titleLen) {
    WinDrawChars(strP_, titleLen, x_, y_);
  } else {
    Int16 titleWidth;
    titleLen = FntWidthToOffset(strP_, titleLen,
				width_ - FntCharWidth(chrEllipsis),
				NULL, &titleWidth);
    WinDrawChars(strP_, titleLen, x_, y_);
    WinDrawChar (chrEllipsis, x_ + titleWidth, y_);
  }
}

static void
ZamaSearch(FindParamsPtr findParams)
{
  MemHandle     recH;
  Char          *headerP;
  Boolean       done, match;
  RectangleType r;
  UInt16        pos, matchLength;
  UInt32        longPos;

  recH = DmGetResource(strRsc, ZAMA_SEARCH_HEADER);
  headerP = MemHandleLock(recH);
  done = FindDrawHeader(findParams, headerP);
  MemHandleUnlock(recH);
  DmReleaseResource(recH);
  if (done) return;
  findParams->more = false;
  if (findParams->recordNum == ZAMA_CODE_ISAKO && EvtSysEventAvail(true)) {
    findParams->more = true;
    return;
  }
  match = TxtFindString("いさこ",
			findParams->strToFind,
			&longPos, &matchLength);
  pos = longPos;
  if (match) {
    done = FindSaveMatch(findParams, ZAMA_CODE_ISAKO,
			 pos, 0, matchLength, 0, 0);
    if (!done) {
      FindGetLineBounds(findParams, &r);
      ZamaSearchDrawName("いさこ", 
			 r.topLeft.x + 1, r.topLeft.y, r.extent.x - 2);
      ++findParams->lineNumber;
    }
  }
}

UInt32
PilotMain(UInt16 cmd, void *cmdPBP, UInt16 launchFlags)
{
  if (cmd == sysAppLaunchCmdNormalLaunch || cmd == sysAppLaunchCmdGoTo) {
    _startup = 0;
    if (cmd == sysAppLaunchCmdGoTo) {
      _startup = ((GoToParamsPtr)cmdPBP)->recordNum;
    }
    if (StartApplication()) {
      FrmGotoForm(ZAMA_MAIN_FORM);
      EventLoop();
      FrmCloseAllForms();
      StopApplication();
    }
  } else if (cmd == sysAppLaunchCmdFind) {
    ZamaSearch((FindParamsPtr)cmdPBP);
  } else if (cmd == sysAppLaunchCmdNotify) {
    SilkHeightChange(cmdPBP);
  }
  return 0;
}
