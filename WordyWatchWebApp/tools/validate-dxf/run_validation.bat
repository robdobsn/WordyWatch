@echo off
REM Windows batch file to run DXF validation
REM Usage: run_validation.bat file.dxf

if "%1"=="" (
    echo Usage: run_validation.bat file.dxf
    echo.
    echo Example: run_validation.bat "C:\path\to\your\file.dxf"
    exit /b 1
)

echo Installing dependencies...
pip install -r requirements.txt

echo.
echo Validating DXF file: %1
echo.

python validate_dxf.py -v "%1"

echo.
echo Validation complete. Press any key to exit.
pause >nul

