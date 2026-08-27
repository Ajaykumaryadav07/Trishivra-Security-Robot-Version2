@echo off
cd /d C:\Users\user\Videos\Trishivra\-TRISH~1\security_robot_v2\build
echo CWD=%CD%
echo COMPILER=C:\msys64\mingw64\bin\c++.exe
echo CMD_START
"C:\msys64\mingw64\bin\c++.exe" -std=gnu++17 -IC:\Users\user\Videos\Trishivra\-TRISH~1\security_robot_v2\include -c ..\src\main.cpp -o main.o 2> err.txt
echo RET=%ERRORLEVEL%
if exist main.o echo MAINOK
if exist err.txt echo ERRFILE_EXISTS
type err.txt
echo CMD_END
