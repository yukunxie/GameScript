@echo off
REM Install VS Code GameScript extension (development mode)
setlocal

echo ========================================
echo   VS Code GameScript Extension Installer
echo ========================================
echo.

cd /d "%~dp0"
echo Current directory: %CD%
echo.

REM Check if VS Code is installed
where code >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] VS Code 'code' command not found.
    echo Please make sure VS Code is installed and added to PATH.
    pause
    exit /b 1
)

echo Installing extension in development mode...
echo.

REM Create a symlink or copy approach for development
set EXT_DIR=%USERPROFILE%\.vscode\extensions\gs-script-tools-dev

echo Removing old development extension...
if exist "%EXT_DIR%" rmdir /S /Q "%EXT_DIR%"

echo Creating extension directory...
mkdir "%EXT_DIR%"

echo Copying files...
xcopy /E /I /Y "%CD%" "%EXT_DIR%" /EXCLUDE:%CD%\exclude.txt >nul

echo.
echo ========================================
echo   ✓ Extension installed successfully!
echo ========================================
echo.
echo Extension location: %EXT_DIR%
echo.
echo Please RESTART VS Code to activate the extension.
echo.
echo To uninstall: Delete folder %EXT_DIR%
echo.
pause
