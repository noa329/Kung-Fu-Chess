# החלטות עיצוב - יומן ריפקטורינג

מסמך זה מתעד החלטות עיצוב לא-טריוויאליות במהלך פירוק הארכיטקטורה
ל-8 השכבות הנדרשות. כל שלב מתועד גם כקומיט נפרד ב-git.

## שלב 1: הפרדת שכבת Movement Rules משכבת Model

**מה נעשה:** הגיאומטריה של תנועת כל כלי (`isValidShape`, `isValidCapture`,
`isSliding`) הוצאה החוצה מהמחלקות `King/Rook/Bishop/Queen/Knight/Pawn`
אל מודול חדש `MovementRules` (namespace עם פונקציות טהורות). ל-`Piece`
נוסף `PieceKind` (enum) בתור מזהה סוג, כדי ש-`MovementRules` יוכל
לבצע dispatch בלי תלות במחלקות ה-Piece עצמן.

**למה:** לפי טבלת הבעלות, Model אחראי רק על "זהות כלים" ואסור לו לדעת
"כללי תנועה". שכבת Movement Rules צריכה להיות ניתנת לבדיקה לגמרי
בבידוד. כעת `MovementRules::isValidShape(kind, color, from, to, rows)`
היא פונקציה טהורה ללא תלות ב-`Board`, ב-`Piece`, בזמן, או ברינדור -
בדיוק כמו שנדרש.

**מה נשאר לעשות בהמשך (במכוון, לא בשלב הזה):**
- `Board::pixelToGrid` ו-`Board::print` עדיין יושבים ב-`Board`, למרות
  שהם שייכים ל-Controller ול-Text I/O בהתאמה. הזזתם עכשיו הייתה שוברת
  קומפילציה כי השכבות האלה עוד לא קיימות. יטופל בשלבים 5 ו-7.
- `Board::isPathClear` נשאר ב-`Board` בתור שאילתת תפוסה גנרית (קריאה
  בלבד, לא מחליטה חוקיות תנועה) - `RuleEngine`/`MovementRules` יקראו לו.
  זו החלטה מכוונת: השארתו כ"תשתית" של Model ולא כחלק מ-Movement Rules.
- `Game` עדיין מכיל את כל שאר השכבות (RuleEngine, GameEngine,
  RealTimeArbiter, Controller) יחד - זה יפורק בשלבים הבאים.

**בדיקות:** `tests/test_movement_rules.cpp` (חדש) - 8 מקרי בדיקה,
25 assertions, מבודדים לגמרי מ-`Piece`/`Board`. `tests/test_pieces.cpp`
הישן הוסר כי הכיסוי שלו עבר במלואו למודול החדש.

## שלב 2: הפרדת שכבת RuleEngine

**מה נעשה:** נוספה מחלקה `RuleEngine` שמחזיקה `const Board&` ומאמתת
חוקיות מהלך מבוקש (צורה דרך `MovementRules` + מסלול פנוי דרך
`Board::isPathClear`) - קריאה בלבד, בלי לשנות כלום. `Game::isMovementLegal`
מאציל אליה את בדיקת "האם המהלך חוקי מבחינת שחמט" ומשאיר אצלו רק את
הבדיקות שקשורות לתנועות ממתינות (`hasPendingMoveOfOppositeColor`,
`hasPendingMoveTo`).

**למה:** לפי טבלת הבעלות, RuleEngine אחראי רק על "אימות חוקיות מהלך
מבוקש, קריאה בלבד" ואסור לו לדעת על תנועות ממתינות/זמן - זה תפקיד
RealTimeArbiter. ההפרדה הזו חושפת בבירור את הגבול: `RuleEngine` ניתן
לבדיקה עם `Board` בלבד (ראו `tests/test_rule_engine.cpp`), בלי שום
תלות ב-`Game` או בזמן מדומה.

**החלטה מכוונת:** `hasPendingMoveOfOppositeColor`/`hasPendingMoveTo`
**נשארו זמנית** ב-`Game`, למרות שלפי הטבלה הם שייכים ל-`RealTimeArbiter`.
הזזתם דורשת קודם להוציא את כל מנגנון ה-pendingMoves/airbornePieces
מ-`Game` (שלב 3) - ניסיון להזיז רק את הבדיקות עכשיו בלי המבנה שמאחוריהן
היה יוצר תלות מעגלית מיותרת. מסומן בקוד בהערה מפורשת.

**בדיקות:** `tests/test_rule_engine.cpp` (חדש) - 5 מקרי בדיקה, 7
assertions, בונות `Board` ישירות ובודקות את `RuleEngine` בבידוד מ-`Game`.
כל 17 מקרי הבדיקה בפרויקט (41 assertions) ירוקים לפני ואחרי.

## שלב 3: הפרדת שכבת RealTimeArbiter

**מה נעשה:** כל מנגנון הזמן-אמת - `PendingMove`, `AirborneState`,
`currentTime`, `calculateTravelTime`, `resolveArrival`,
`finalizeReadyMoves`, `finalizeAirborne` - יצא מ-`Game` למחלקה חדשה
`RealTimeArbiter`. `RealTimeArbiter` מחזיק `Board&` (לא const - הוא
כן מזיז כלים בפועל בעת פתרון הגעה) ומספק API: `scheduleMove`,
`scheduleJump`, `hasPendingMoveFrom/To/OfOppositeColor`, `isAirborne`,
ו-`advance(ms)` שמקדם את הזמן ומחזיר `vector<CaptureEvent>`.

**למה:** לפי טבלת הבעלות, RealTimeArbiter אחראי על "אובייקטי Motion
פעילים, קידום זמן מדומה, פתרון הגעה, ואירועי אכילה" ואסור לו להחליט
"המשחק נגמר" - זו אחריות GameEngine. לכן `resolveArrival` **לא** קובע
`gameOver` בעצמו יותר; הוא מדווח `CaptureEvent{at, capturedColor,
wasKing}` והחלטת "אכילת מלך מסיימת משחק" עברה ל-`Game::applyCaptureEvents`
(תפקיד ה-GameEngine, שעדיין גר בתוך `Game` - יופרד רשמית בשלב הבא).

**החלטה מכוונת נוספת:** הרחבתי מעט את הדיווח - כל אכילה (לא רק אכילת
מלך) מייצרת כעת `CaptureEvent`, לא רק כשהמלך נאכל. זה לא שינוי התנהגות
בלוח (הלוח מתנהג זהה לחלוטין - כל הבדיקות הישנות עברו ללא שינוי), רק
תשתית דיווח כללית יותר שתשרת בעתיד תכונות כמו לוג אכילות/מונה נקודות,
בלי לדרוש שינוי חוזי נוסף ב-API.

**בדיקות:** `tests/test_real_time_arbiter.cpp` (חדש) - 8 מקרי בדיקה,
19 assertions, כולל המקרה העדין של אכילת כלי מרחף באוויר ואת דיווח
אכילת המלך. כל 25 מקרי הבדיקה בפרויקט (60 assertions) ירוקים לפני
ואחרי, והרצתי גם בדיקת עשן (`Board:`/`Commands:` דרך stdin) על
הבינארי האמיתי כדי לוודא שההתנהגות מקצה לקצה זהה למה שהייתה.

## שלב 4: הפיכת Game ל-GameEngine אמיתי (API בלי פיקסלים + snapshot)

**מה נעשה:** שינוי שם המחלקה `Game`→`GameEngine`, ושינוי ה-API הציבורי
מפיקסלים (`click(int x, int y)`) לקואורדינטות לוח (`select(const
Position&)`). נוסף `GameSnapshot` (טוקנים של הלוח + `selected` +
`gameOver`) ומתודת `snapshot() const` לקריאה בלבד - הכנה ישירה
ל-Renderer (שלב 6). נוספה גם `Board::getColCount()` (תוספת קטנה
לשכבת Model, נדרשת כדי ש-`snapshot()` ידע את רוחב הלוח).

**למה:** הטבלה אוסרת על GameEngine "פירוש קלט... או מיפוי פיקסלים".
כל עוד `click` קיבל `(x, y)` וקרא ל-`board.pixelToGrid`, GameEngine
הפר את הכלל הזה. הסרת הפיקסלים מה-API הפכה את השכבה לתואמת לגמרי,
והיא גם ההכנה הישירה ל-Controller: הוא יהיה בפועל רק "תרגם פיקסל,
תעביר ל-GameEngine".

**החלטה מכוונת (חוב טכני מתועד, זמני):** מכיוון ש-Controller אמיתי
עוד לא קיים (שלב 5), `main.cpp` מכיל כרגע פונקציית `pixelToGrid`
מקומית וזמנית שמשכפלת את הנוסחה שכבר קיימת ב-`Board::pixelToGrid`
(עם אותו גודל תא ברירת מחדל, 100). זה שכפול קוד מכוון ומתועד -
הוא יוסר בשלב 5 כש-Controller אמיתי יאמץ את האחריות הזו. הבחירה
היא להשאיר את הכפילות הקטנה הזו זמנית, ולא "להברי" מיפוי פיקסלים
בחזרה ל-GameEngine רק כדי לחסוך אותה - כי זה היה מבטל את כל הטעם
של השלב הזה.

**בדיקות:** `tests/test_game_engine.cpp` (חדש, מחליף את
`tests/test_game.cpp`) - 4 מקרי בדיקה, 6 assertions, עובדות על
`Position` ישירות ובודקות גם את ה-`snapshot()` (כולל `gameOver`
ו-`selected`). נוסף גם מקרה בדיקה ל-`Board::getColCount`. כל 28
מקרי הבדיקה בפרויקט (66 assertions) ירוקים, וגם כאן וידאתי מול
הבינארי האמיתי (עם `main.cpp` המעודכן) שהפלט זהה לחלוטין למה שהיה
לפני הריפקטור.

## שלב 5: Controller - סגירת חוב הפיקסלים משלבים 1 ו-4

**מה נעשה:** מחלקה חדשה `Controller` שמחזיקה `GameEngine&` וממירה
פיקסל→`Position` (`handleClick`/`handleJump`), ואז קוראת ל-
`GameEngine::select`/`jump`. `Board::pixelToGrid` (וה-`cellPixelSize`
שתמך בו) **הוסרו לגמרי** מ-`Board` - זו הייתה נקודה פתוחה שתועדה
מפורשות בשלב 1 ("יטופל בשלב 5"). גם הפונקציה הזמנית שישבה ב-`main.cpp`
משלב 4 הוסרה; `main.cpp` בונה כעת `Controller` אמיתי.

**למה:** לפי הטבלה, Controller אחראי על "פירוש קליקים ומצב תא נבחר"
ואסור לו חוקיות שחמט, שינוי Board, רינדור, או תזמון. עכשיו נקודת
הכניסה היחידה בכל הפרויקט שממירה פיקסלים לקואורדינטת לוח היא
`Controller::pixelToGrid` (private) - לא ב-Model ולא ב-GameEngine.

**החלטת עיצוב לא-טריוויאלית - "מצב תא נבחר":** הטבלה אומרת ש-Controller
בעלים גם על "מצב תא נבחר". בפועל `GameEngine` עדיין מחזיק את השדה
`selected` הפנימי שלו, כי הוא חלק בלתי נפרד מהלוגיקה של "קליק ראשון
בוחר, קליק שני מנסה מהלך" שכבר מכוסה בבדיקות קיימות - לפרק את זה
היה דורש שינוי חוזה מהותי ב-API בלי תועלת מוכחת. הפתרון שבחרתי: מצב
הבחירה **חשוף לקריאה בלבד** דרך `GameEngine::snapshot().selected`
(שכבר קיים משלב 4), ו-Controller לא מחזיק עותק כפול משלו - הוא רק
מתרגם פיקסלים. כך "מצב תא נבחר" זמין לכל צרכן חיצוני (כמו Renderer
בשלב הבא) בלי כפילות state בין שתי שכבות.

**בדיקות:** `tests/test_controller.cpp` (חדש) - 4 מקרי בדיקה,
כולל התנהגות זהה לגמרי למה שהיה ב-`Game::click` הישן (כולל תמיכה
בגודל תא מותאם אישית). `tests/test_board.cpp` איבד את מקרה הבדיקה
של `pixelToGrid` (המעבר תועד ב-`test_controller.cpp` במקום). כל 31
מקרי הבדיקה בפרויקט (66 assertions) ירוקים, ווידאתי גם מול הבינארי
האמיתי שהפלט זהה לגמרי לפני ואחרי.

## שלב 6: Renderer

**מה נעשה:** מחלקה `Renderer` עם מתודה יחידה `render(const
GameSnapshot&, std::ostream&) const` - ציור טקסטואלי-חזותי (עם
קואורדינטות ותא נבחר מסומן ב-`[..]`) מ-snapshot בלבד, קריאה בלבד.

**למה/החלטה מכוונת:** בפרויקט הזה **אין ספריית ציור גרפית מסופקת**
בפועל (זו הייתה הנחת הבסיס בדרישות המקוריות - "ציור באמצעות ספריית
הציור שסופקה"). לכן המימוש כאן הוא תחליף טקסטואלי-חזותי לקונסולה,
עומד באותו חוזה בדיוק (`GameSnapshot` בכניסה, שום תלות בכלל אחר) -
כך שאם בעתיד תסופק ספריית ציור אמיתית (למשל SFML/SDL), רק תוכן
הפונקציה `render()` ישתנה; שום שכבה אחרת בפרויקט לא תדע על כך. זו
בדיוק המשמעות של "שכבת הצגה דקה וניתנת להחלפה" שהמנחה דרש.

**הבחנה חשובה מ-Text I/O (השלב הבא):** `Renderer` **אינו** תחליף
ל-`BoardPrinter` העתידי. `Renderer` מייצר תצוגה קריאה-לאדם;
`BoardPrinter` (Text I/O) ידפיס טוקנים לוגיים מדויקים (כמו הפורמט
של `Board::print()` הישן) לצורך השוואה מדויקת ב-TextTestRunner. שני
תפקידים שונים לגמרי, ולכן שתי מחלקות שונות - לא הרחבה של אותה אחת.

**בדיקות:** `tests/test_renderer.cpp` (חדש) - 3 מקרי בדיקה, בונות
`GameSnapshot` ידנית ובודקות טקסט הפלט. `Renderer` לא נדרש לחיבור
ל-`main.cpp` בשלב הזה, כי פרוטוקול הטקסט הקיים (`print board`)
דורש את הפלט המדויק של `BoardPrinter`, לא תצוגה חזותית - זה יטופל
בשלב 7. כל 34 מקרי הבדיקה בפרויקט (70 assertions) ירוקים.

## שלב 7: Text I/O (BoardParser + BoardPrinter) - וגם TextTestRunner בפועל

**מה נעשה:** שני מודולים חדשים: `BoardParser::parse(std::istream&)`
(פירוש קטע "Board:" לטוקנים, כולל שגיאות `ERROR UNKNOWN_TOKEN`/
`ERROR ROW_WIDTH_MISMATCH` - הועתק מ-`main.cpp`) ו-`BoardPrinter::print`
(הדפסת טוקנים לוגיים בפורמט מדויק - הועתק מ-`Board::print()`).
`Board::print()` **הוסר** מ-Model (רינדור אסור ל-Model), ו-
`GameEngine::printBoard()` **הוסר** גם הוא (רינדור אסור ל-GameEngine
- זו הייתה נקודה זמנית מתועדת מפורש משלב 4, "יוסר בשלב 7"). `main.cpp`
עודכן לקרוא ל-`BoardParser`/`BoardPrinter` במקום ללוגיקה מקומית,
ובכך גם ממלא בפועל את תפקיד ה-TextTestRunner מהטבלה (הרצת פקודות
מקטע "Commands:" דרך ה-API הציבורי של Controller/GameEngine בלבד,
בלי UI אמיתי) - נקודה זו תועדה בראש הקובץ.

**למה:** לפי הטבלה, Text I/O אחראי בדיוק על "BoardParser ו-
BoardPrinter: הגדרה טקסטואלית ופלט לוח לוגי" ואסור לו כללי תנועה,
ביצוע פקודות, רינדור, או בדיקות מעבר להשוואת טקסט. TextTestRunner
אחראי על "פירוש סקריפט והפעלת נתיב הפקודות הציבורי" - וזה בדיוק מה
ש-`main.cpp` עושה עכשיו, בלי שום לוגיקת פענוח לוח/הדפסה מקומית.

**החלטה מכוונת:** לא פיצלתי את `main.cpp` למחלקת `TextTestRunner`
נפרדת (קובץ .hpp/.cpp) בשלב הזה - זו כרגע פונקציית `main` דקה שקוראת
רק ל-API הציבורי של שכבות אחרות (`BoardParser`, `Controller`,
`GameEngine::wait`, `BoardPrinter`), כלומר היא כבר עומדת בדרישת
"אין שכפול לוגיקת משחק" בפועל. אם בעתיד יהיה צורך להריץ כמה תסריטי
בדיקה טקסטואליים בתוך תהליך בדיקה אחד (למשל מ-doctest), שווה להוציא
את הלולאה הזו למחלקה/פונקציה עצמאית - אך זה שינוי קל וללא סיכון
כשיהיה צורך אמיתי בו, ולא לפניו.

**בדיקות:** `tests/test_text_io.cpp` (חדש) - 4 מקרי בדיקה, 7
assertions, על `BoardParser`/`BoardPrinter` בבידוד מ-`main`. כל 38
מקרי הבדיקה בפרויקט (77 assertions) ירוקים, ווידאתי גם ידנית מול
הבינארי האמיתי - כולל שני מסלולי השגיאה (`UNKNOWN_TOKEN`,
`ROW_WIDTH_MISMATCH`) - שהפלט זהה לחלוטין למה שהיה לפני הריפקטור.

## סיכום: כל 8 השכבות קיימות כעת

Model (`Board`/`Piece`/`Pieces`/`Position`/`PieceKind`), Movement Rules,
RuleEngine, RealTimeArbiter, GameEngine, Controller, Renderer, ו-Text
I/O (עם `main.cpp` ממלא את תפקיד TextTestRunner). הצעד הפתוח היחיד
שנשאר הוא ארגון מחדש של `include`/`src` לתיקיות-לפי-שכבה, שהוחלט
מראש לדחות לסוף הריפקטור (ראו שיחה) כדי לא לערבב שינויי מבנה עם
שינויי לוגיקה בקומיטים.
