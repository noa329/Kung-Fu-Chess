#ifndef MOVEMENT_RULES_H
#define MOVEMENT_RULES_H
#include "Position.hpp"
#include "PieceKind.hpp"

// שכבת Movement Rules (ראו טבלת בעלות השכבות):
// בעלות: גיאומטריית תנועה לכל סוג כלי, מחושבת מנתוני קלט טהורים (kind, color, from, to, boardRows).
// אסור לה: פקודות משחק, זמן חלוף, אנימציה, רינדור, או טיפול בקלט.
//
// המימוש הפנימי הוא Strategy pattern: כל כלי הוא מחלקה נפרדת שמממשת
// IMovementStrategy (ראו movement_rules/*.hpp), ו-MovementRules הוא
// פאסאד דק מעליהן שמפנה ל-getMovementStrategy(kind). ה-API הציבורי
// כאן לא השתנה - קוד קורא קיים (RuleEngine ואחרים) לא מרגיש הבדל.
namespace MovementRules {
    // האם הצורה הגיאומטרית של from->to חוקית לסוג כלי זה (בלי אכילה).
    bool isValidShape(PieceKind kind, char color, const Position& from, const Position& to, int boardRows);

    // האם הצורה הגיאומטרית של from->to חוקית לאכילה עבור סוג כלי זה.
    // ברירת המחדל לרוב הכלים זהה ל-isValidShape; לחייל יש כלל אכילה שונה.
    bool isValidCapture(PieceKind kind, char color, const Position& from, const Position& to, int boardRows);

    // האם סוג כלי זה דורש מסלול פנוי (שימושי עבור RuleEngine שיבדוק isPathClear בלוח).
    bool isSliding(PieceKind kind);
}
#endif
