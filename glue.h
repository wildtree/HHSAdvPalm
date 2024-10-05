/*
 * Compalibility Library for PalmOS 3.5/4/5
 */

#ifndef GLUE_H
#define GLUE_H

Boolean GlueBmpIsCompressed(BitmapPtr);
void    GlueBmpGetDimensions(BitmapPtr, Coord*, Coord*, UInt16*);
void    GlueBmpGetSizes(BitmapPtr, UInt32*, UInt32*);

#endif /* GLUE_H */
