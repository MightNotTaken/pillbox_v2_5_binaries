@echo off

REM Delete old bin if it exists
if exist v2_5.ino.bin del v2_5.ino.bin

REM Copy new bin from build folder
copy C:\tahir\codes\mann-medicenter\v2_5\build\esp32.esp32.esp32\v2_5.ino.bin .

REM Git sync
git add .
git commit -m "sync"
git push origin main

pause
