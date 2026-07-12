#ifndef PIECE_KIND_H
#define PIECE_KIND_H

// זהות סוג הכלי (Model). לא מכיל שום דבר על איך הכלי זז - זה תפקידה
// של שכבת Movement Rules.
enum class PieceKind { King, Queen, Rook, Bishop, Knight, Pawn };

#endif
