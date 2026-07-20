#!/usr/bin/env bash
# בונה חבילת הגשה שטוחה ל-VPL: כל קבצי הייצור (include/**/*.hpp,
# src/**/*.cpp, main.cpp) בתיקייה אחת ללא תתי-תיקיות, בלי בדיקות
# ובלי doctest.h. VPL לא מוסיף -I לתתי-תיקיות בזמן קומפילציה, אז
# כל הקבצים חייבים לשבת יחד לצד כל ה-#include-ים בשם-קובץ-בלבד
# שכבר קיימים בקוד (ראו ARCHITECTURE_DECISIONS.md).
set -euo pipefail
cd "$(dirname "$0")/.."

OUT_DIR="vpl_submission"
ZIP_NAME="kung_fu_chess_vpl.zip"

rm -rf "$OUT_DIR" "$ZIP_NAME"
mkdir -p "$OUT_DIR"

find include -name "*.hpp" -exec cp {} "$OUT_DIR"/ \;
find src -name "*.cpp" -exec cp {} "$OUT_DIR"/ \;
cp main.cpp "$OUT_DIR"/

# third_party/miniaudio/miniaudio.h: pulled in bare (#include "miniaudio.h")
# by src/audio/SoundManager.cpp, same flat-directory/bare-filename convention
# as everything else here - it isn't under include/ so the *.hpp glob above
# doesn't catch it (also .h, not .hpp).
cp third_party/miniaudio/miniaudio.h "$OUT_DIR"/

count=$(ls "$OUT_DIR" | wc -l)
echo "Packaged $count files into $OUT_DIR (VPL limit: 70)"

(cd "$OUT_DIR" && zip -q "../$ZIP_NAME" *.hpp *.cpp *.h)
echo "Created $ZIP_NAME - upload this to VPL"
