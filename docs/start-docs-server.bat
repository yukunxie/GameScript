@echo off
setlocal ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

set "PORT=5500"
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

rem If the default port already hosts docs, reuse it.
call :is_port_listening %PORT%
if !errorlevel!==0 (
  call :is_docs_home_ok %PORT%
  if !errorlevel!==0 (
    set "HOME_URL=http://localhost:%PORT%/index.html"
    echo [docs] Docs server already running on port %PORT%.
    start "" "%HOME_URL%"
    goto :eof
  )
  echo [docs] Port %PORT% is occupied by a non-docs service. Searching another port...
)

rem Resolve an actual python.exe path to avoid launcher edge cases.
set "PYTHON_EXE="
for /f "usebackq delims=" %%I in (`py -3 -c "import sys; print(sys.executable)" 2^>nul`) do (
  if not defined PYTHON_EXE set "PYTHON_EXE=%%I"
)
if not defined PYTHON_EXE (
  for /f "usebackq delims=" %%I in (`python -c "import sys; print(sys.executable)" 2^>nul`) do (
    if not defined PYTHON_EXE set "PYTHON_EXE=%%I"
  )
)

if not defined PYTHON_EXE (
  echo [docs] ERROR: Python not found in PATH.
  echo [docs] Install Python or start a static server manually in: %SCRIPT_DIR%
  exit /b 1
)

rem Choose the first available port from a small range.
set "PORT="
for %%P in (5500 5501 5502 5503 5504 5505 5506 5507 5508 5509 5510) do (
  call :is_port_listening %%P
  if !errorlevel! neq 0 (
    if not defined PORT set "PORT=%%P"
  )
)

if not defined PORT (
  echo [docs] ERROR: No free port found in range 5500-5510.
  exit /b 1
)

set "HOME_URL=http://localhost:%PORT%/index.html"

echo [docs] Starting docs server on port %PORT%...
start "GameScript Docs Server" /min "%PYTHON_EXE%" -m http.server %PORT% --directory "%SCRIPT_DIR%"

rem Wait up to 10 seconds for the port to start listening and docs homepage to be reachable.
set "STARTED=0"
for /l %%T in (1,1,10) do (
  call :is_port_listening %PORT%
  if !errorlevel!==0 (
    call :is_docs_home_ok %PORT%
    if !errorlevel!==0 (
      set "STARTED=1"
      goto :open_home
    )
  )
  timeout /t 1 /nobreak >nul
)

if "%STARTED%"=="0" (
  echo [docs] ERROR: Server process did not start listening on port %PORT%.
  echo [docs] Try running manually:
  echo [docs]   "%PYTHON_EXE%" -m http.server %PORT% --directory "%SCRIPT_DIR%"
  exit /b 1
)

:open_home
start "" "%HOME_URL%"

echo [docs] Opened %HOME_URL%
exit /b 0

:is_port_listening
set "CHECK_PORT=%~1"
netstat -ano | findstr ":%CHECK_PORT%" | findstr "LISTENING" >nul
if %errorlevel%==0 (exit /b 0) else (exit /b 1)

:is_docs_home_ok
set "CHECK_PORT=%~1"
powershell -NoProfile -Command "try { $r = Invoke-WebRequest -UseBasicParsing -Uri 'http://localhost:%CHECK_PORT%/index.html' -TimeoutSec 2; if ($r.StatusCode -eq 200) { exit 0 } else { exit 1 } } catch { exit 1 }" >nul 2>&1
if %errorlevel%==0 (exit /b 0) else (exit /b 1)
