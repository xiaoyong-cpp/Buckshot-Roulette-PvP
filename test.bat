@echo off
chcp 65001 >nul
rem ============================================================================
rem 无头逻辑测试编译脚本（不依赖 raylib/Winsock，控制台程序）
rem 验证核心游戏规则：弹仓、回合、道具、胜利条件、N 人支持
rem ============================================================================
cd /d "%~dp0"

echo [1/2] 收集测试所需源文件...
del test_build.rsp 2>nul
echo "headless_test.cpp" >> test_build.rsp
for %%d in (core items modes utils) do (
    for %%f in (src\%%d\*.cpp) do @echo "%%~ff" >> test_build.rsp
)

echo [2/2] 编译无头测试...
g++ -std=c++20 -O2 @test_build.rsp -o headless_test.exe
if errorlevel 1 (
    echo.
    echo [失败] 编译出错，请检查上方错误信息
    del test_build.rsp 2>nul
    pause
    exit /b 1
)

echo.
echo [成功] 编译完成，运行测试...
echo ========================================
headless_test.exe
set rc=%errorlevel%
echo ========================================
if %rc%==0 (
    echo [全部通过]
) else (
    echo [存在失败用例]
)
del test_build.rsp 2>nul
pause
