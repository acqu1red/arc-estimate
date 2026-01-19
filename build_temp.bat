@echo off
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" estimate1.vcxproj /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal
