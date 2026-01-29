@echo off
setlocal enabledelayedexpansion

REM ==========================================
REM НАСТРОЙКИ ОКРУЖЕНИЯ
REM ==========================================

REM Добавляем путь к CMake (как вы просили)
set PATH=C:\Qt\Tools\CMake_64\bin;%PATH%

REM ==========================================
REM НАСТРОЙКИ ПРОЕКТА
REM ==========================================

REM Список проектов
set PROJECTS=CommandApp EmulatorApp EvoLiteApp EvoTestApp IndicatorApp ModbusApp

REM Имя папки временной сборки (внутри каждого проекта)
set BUILD_DIR=build_release

REM Имя ОБЩЕЙ папки, куда соберутся все exe и dll (создается в корне репозитория)
set FINAL_DIR=%~dp0Evo_Release_All

REM ==========================================
REM НАЧАЛО РАБОТЫ
REM ==========================================

echo [Setup] CMake path set to: C:\Qt\Tools\CMake_64\bin
echo [Setup] Creating final directory: %FINAL_DIR%

if not exist "%FINAL_DIR%" mkdir "%FINAL_DIR%"

for %%P in (%PROJECTS%) do (
    echo.
    echo ------------------------------------------
    echo Processing project: %%P
    echo ------------------------------------------
    
    if exist "%%P" (
        pushd "%%P"
        
        REM --- 1. CONFIGURATION ---
        echo [CMake] Configuring...
        cmake -S . -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release
        if errorlevel 1 goto ErrorHandler

        REM --- 2. BUILD ---
        echo [CMake] Building...
        cmake --build "%BUILD_DIR%" --config Release
        if errorlevel 1 goto ErrorHandler

        REM --- 3. FIND EXE ---
        set EXE_FOUND=0
        set "SOURCE_EXE_PATH="
        
        REM Проверка пути для MSVC (Visual Studio)
        if exist "%BUILD_DIR%\Release\%%P.exe" (
            set "SOURCE_EXE_PATH=%BUILD_DIR%\Release\%%P.exe"
            set EXE_FOUND=1
        )
        
        REM Проверка пути для MinGW
        if !EXE_FOUND! EQU 0 (
            if exist "%BUILD_DIR%\%%P.exe" (
                set "SOURCE_EXE_PATH=%BUILD_DIR%\%%P.exe"
                set EXE_FOUND=1
            )
        )

        REM --- 4. COPY & DEPLOY ---
        if !EXE_FOUND! EQU 1 (
            echo [Copy] Copying %%P.exe to final directory...
            copy /Y "!SOURCE_EXE_PATH!" "%FINAL_DIR%\" >nul
            
            echo [Windeployqt] Deploying dependencies into Final Directory...
            REM Запускаем деплой уже на общем exe файле. 
            REM Он не будет дублировать DLL, если они там уже есть.
            windeployqt --release --compiler-runtime --no-translations "%FINAL_DIR%\%%P.exe"
            
            echo [Success] %%P is ready in common folder.
        ) else (
            echo [Error] Could not find %%P.exe inside build folder.
        )

        popd
    ) else (
        echo [Warning] Directory %%P not found!
    )
)

echo.
echo ==========================================
echo All tasks finished successfully.
echo Result folder: %FINAL_DIR%
echo ==========================================
pause
exit /b 0

:ErrorHandler
echo.
echo [FATAL ERROR] Build failed inside %%P
popd
pause
exit /b 1