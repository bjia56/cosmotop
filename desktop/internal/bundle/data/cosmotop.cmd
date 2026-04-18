@echo off
setlocal

echo This is a placeholder. Press q to continue...

:wait_loop
set /p key=
if /i "%key%"=="q" goto done
goto wait_loop

:done
endlocal
