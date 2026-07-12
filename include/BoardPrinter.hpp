#ifndef BOARD_PRINTER_H
#define BOARD_PRINTER_H
#include <vector>
#include <string>
#include <ostream>

// שכבת Text I/O (ראו טבלת בעלות השכבות):
// בעלות: הדפסת מצב לוח לוגי.
// אסור לה: כללי תנועה, ביצוע פקודות, רינדור חזותי, או לוגיקת בדיקות
// מעבר להשוואת טקסט.
//
// שימו לב: זה שונה מ-Renderer (שכבה נפרדת) - BoardPrinter מדפיס
// טוקנים לוגיים מדויקים, בפורמט קבוע, לצורך השוואה מדויקת של פלט
// (למשל ע"י TextTestRunner). Renderer מייצר תצוגה קריאה-לאדם.
namespace BoardPrinter {
    void print(const std::vector<std::vector<std::string>>& tokens, std::ostream& out);
}
#endif
