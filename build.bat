@echo off
setlocal

REM Попытка найти MSBuild
set MSBUILD_PATH=
for %%i in (
    "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe"
) do (
    if exist %%i (
        set MSBUILD_PATH=%%i
        goto :found
    )
)

echo MSBuild not found!
exit /b 1

:found
echo Using MSBuild: %MSBUILD_PATH%
"%MSBUILD_PATH%" estimate1.vcxproj /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal
