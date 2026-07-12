#ifndef BOARD_PARSER_H
#define BOARD_PARSER_H
#include <vector>
#include <string>
#include <istream>

// שכבת Text I/O (ראו טבלת בעלות השכבות):
// בעלות: פירוש הגדרת לוח טקסטואלית.
// אסור לה: כללי תנועה, ביצוע פקודות, רינדור, או לוגיקת בדיקות מעבר
// להשוואת טקסט.
struct BoardParseResult {
    bool ok;
    std::vector<std::vector<std::string>> tokens;
    std::string error; // תקף רק כש-ok == false
};

namespace BoardParser {
    bool isValidToken(const std::string& token);

    // קורא שורות עד "Commands:" (או EOF) ומפרש אותן לטוקנים.
    // לא צורך את שורת ה-"Commands:" עצמה, אבל כן מדלג עליה מהזרם.
    BoardParseResult parse(std::istream& in);
}
#endif
