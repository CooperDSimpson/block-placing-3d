@echo off
setlocal enabledelayedexpansion

:: === Config paths (adjust to where you unzipped) ===
set GLFW_INC="C:\glfw\include"
set GLFW_LIB="C:\glfw\lib-mingw-w64"

:: === Common compiler/linker flags ===
set CFLAGS=-I%GLFW_INC% 
set LFLAGS=-L%GLFW_LIB% -lglfw3 -lopengl32 -lgdi32 -luser32 -lglu32 -lwinmm

:: === Count cpp files ===
set count=0
for %%f in (*.cpp) do set /a count+=1

if %count% equ 0 (
    echo No .cpp files found!
    pause
    goto :eof
)

if %count% equ 1 (
    for %%f in (*.cpp) do (
        echo Building %%f...
        g++ "%%f" ../engine-thingy/glad/glad.c -o "%%~nf.exe" %CFLAGS% %LFLAGS%
    )
) else (
    echo Multiple .cpp files found:
    set /a i=1
    for %%f in (*.cpp) do (
        echo !i!. %%f
        set file!i!=%%f
        set /a i+=1
    )
    set /p choice="Enter number (1-%count%): "
    if defined file%choice% (
        set selectedfile=!file%choice%!
        echo Building !selectedfile!...
        g++ "!selectedfile!" -o "!selectedfile:~0,-4!.exe" %CFLAGS% %LFLAGS%
    ) else (
        echo Invalid selection!
    )
)

endlocal
