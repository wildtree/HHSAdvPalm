/*
 * High High School Adventure
 *
 *    Copyright(c)1985 ZOBplus
 *    Copyright(c)2002 ZOBplus hiro <hiro@zob.ne.jp>
 */

#ifndef HIGH_H
#define HIGH_H

#define ZAMA_CREATOR_ID   'ZAMA'
#define ZAMA_PREFS_ID     (0)
#define ZAMA_PREFS_VER    (100)

#define ZAMA_ITEM_NUMBERS (12)
#define ZAMA_CMD_INVALID  (0)
#define ZAMA_CMD_END      (61)

#define ZAMA_SILK_NOTIFY  (1)

#define ZAMA_CODE_ISAKO   (135)

enum ZamaDirection {
  DirInvalid = -1,
  DirNorth, DirSouth, DirWest, DirEast, DirUp, DirDown, DirEnter, DirExit,
  DirMax
};

enum ZamaSex {
  SexInvalid = 0,
  SexBoy,
  SexGirl,
  SexMax
};

enum ActType {
  ActComp = 0,
  ActAction
};

enum CmpOperator {
  CMP_NOP = 0,
  CMP_EQ,
  CMP_NE,
  CMP_GT,
  CMP_GE,
  CMP_LT,
  CMP_LE,
};

enum ActOperator {
  ACT_NOP = 0,
  ACT_MOVE,
  ACT_ASGN,
  ACT_MESG,
  ACT_DLOG,
  ACT_LOOK,
  ACT_SND,
  ACT_OVER,
};

enum RuleValueType {
  TYPE_NONE = 0,
  TYPE_FACT,
  TYPE_PLACE,
  TYPE_SYSTEM,
  TYPE_VECTOR,
};

typedef struct CmdHeaderType {
  UInt16 _act:1;
  UInt16 _op:3;
  UInt16 _pad:4;
  UInt16 _type:3;
  UInt16 _id:5;
} CmdHeaderType;
typedef CmdHeaderType *CmdHeaderPtr;

typedef struct CmdBodyType {
  UInt16 _type:3;
  UInt16 _id:5;
  UInt16 _value:8;
} CmdBodyType;
typedef CmdBodyType *CmdBodyPtr;

typedef struct CmdBlkType {
  CmdHeaderType _head;
  CmdBodyType   _body;
} CmdBlkType;
typedef CmdBlkType *CmdBlkPtr;

typedef struct RuleType {
  UInt8 _mapId;
  UInt8 _cmdN;
  UInt8 _objN;
  UInt8 _pad;
  UInt8 _args[44];
} RuleType;
typedef RuleType *RulePtr;

#define EndOfRule (0xff)

typedef struct ZamaPrefsType {
  UInt8   _lastSaved;
  Boolean _suspended;
  FontID  _curFont;
  Boolean _linedraw;
  Boolean _sound;
} ZamaPrefsType;
typedef ZamaPrefsType *ZamaPrefsPtr;

typedef struct ZamaMapType {
  UInt8 graphic[0x400];
  UInt8 reaction[0x100];
  UInt8 mesg[0x300];
} ZamaMapType;
typedef ZamaMapType *ZamaMapPtr;

typedef struct ZamaUserDataType {
  UInt8   vector[87][8];
  UInt8   place[ZAMA_ITEM_NUMBERS];
  UInt8   fact[15];
  union {
    struct {
      UInt8 mapID, mapW, cmdN, objN, dret, rand, dlogOk, dmesg;
    } n;
    UInt8 ary[8]; /* 0: mapID 1: mapW  2: cmdN 3: objN 4: dret 5: rand */
                  /* 6: dlogOk 7: dmesg 8: reserved */
  } sys;
} ZamaUserDataType;
typedef ZamaUserDataType *ZamaUserDataPtr;

typedef struct ZamaWordType {
  Char  word[4];
  UInt8 number;
} ZamaWordType;
typedef ZamaWordType *ZamaWordPtr;

typedef struct ZamaMesgType {
  UInt8 len;
  Char  mesg[1];
} ZamaMesgType;
typedef ZamaMesgType *ZamaMesgPtr;

typedef struct ZamaReactionType {
  UInt16 action;
  UInt8  number;
} ZamaReactionType;
typedef ZamaReactionType *ZamaReactionPtr;

#ifndef stdSilkHeight
#define stdSilkHeight (65)
#endif /* stdSilkHeight */

#endif /* HIGH_H */
