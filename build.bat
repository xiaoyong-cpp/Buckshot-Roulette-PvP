@echo off
chcp 65001 >nul
rem ============================================================================
rem 恶魔轮盘编译脚本（MinGW-w64 + raylib 5.5）
rem 依赖均已安装在 MinGW 包含目录，无需额外路径配置
rem 用法：双击本文件，或在项目根目录执行 build.bat
rem ============================================================================
cd /d "%~dp0"

echo [1/2] 收集源文件...
del build.rsp 2>nul
for /R src %%f in (*.cpp) do @echo "%%~ff" >> build.rsp

echo [2/2] 编译链接中（可能需要 1~2 分钟）...
g++ -std=c++20 -O2 -mwindows @build.rsp -o BuckshotRoulette.exe -lraylib -lws2_32 -lgdi32 -lopengl32 -lwinmm -static
if errorlevel 1 (
    echo.
    echo [失败] 编译出错，请检查上方错误信息
    del build.rsp 2>nul
    pause
    exit /b 1
)

echo.
echo [成功] 已生成 BuckshotRoulette.exe，双击即可运行（零命令行操作）
del build.rsp 2>nul
pause
