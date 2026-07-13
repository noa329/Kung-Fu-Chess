#ifndef RENDERER_H
#define RENDERER_H
#include <ostream>
#include "GameEngine.hpp"

// שכבת Renderer (ראו טבלת בעלות השכבות):
// בעלות: ציור חזותי מ-GameSnapshot לקריאה בלבד.
// אסור לה: כללי משחק, שינוי Board, פירוש קלט, או לוגיקת בדיקות-טקסט.
//
// הערה: בפרויקט הזה לא סופקה ספריית ציור גרפית - המימוש כאן מייצר
// ייצוג טקסטואלי-חזותי לקונסולה (עם קואורדינטות וסימון תא נבחר),
// כתחליף דק וניתן-להחלפה. שים לב שזה *לא* אותו דבר כמו BoardPrinter
// (שכבת Text I/O) - BoardPrinter מדפיס טוקנים לוגיים מדויקים
// להשוואת TextTestRunner, ואילו Renderer מייצר תצוגה קריאה לאדם.
class Renderer {
public:
    void render(const GameSnapshot& snapshot, std::ostream& out) const;
};
#endif
