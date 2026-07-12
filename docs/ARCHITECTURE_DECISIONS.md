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
