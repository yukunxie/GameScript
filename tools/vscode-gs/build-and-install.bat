@echo off
REM Auto-build and install VS Code GameScript extension
setlocal

echo ========================================
echo   VS Code GameScript Extension Builder
echo ========================================
echo.

cd /d "%~dp0"
echo [1/5] Current directory: %CD%
echo.

REM Check if vsce is installed
where vsce >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] vsce not found. Installing globally...
    call npm install -g @vscode/vsce
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] Failed to install vsce. Please run: npm install -g @vscode/vsce
        pause
        exit /b 1
    )
)

echo [2/5] Removing old VSIX packages...
if exist "*.vsix" del /Q *.vsix
echo.

echo [3/5] Packaging extension...
call vsce package --no-yarn
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to package extension.
    pause
    exit /b 1
)
echo.

echo [4/5] Finding VSIX file...
for %%f in (*.vsix) do set VSIX_FILE=%%f
if not defined VSIX_FILE (
    echo [ERROR] VSIX file not found.
    pause
    exit /b 1
)
echo Found: %VSIX_FILE%
echo.

echo [5/5] Installing extension to VS Code...
code --install-extension "%VSIX_FILE%" --force
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to install extension.
    pause
    exit /b 1
)
echo.

echo ========================================
echo   ✓ Extension installed successfully!
echo ========================================
echo.
echo Please restart VS Code to see the changes.
echo VSIX location: %CD%\%VSIX_FILE%
echo.
pause
