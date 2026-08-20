@echo off
setlocal
title OPENNEOUA MOTORE - GIT GC SICURO

rem Lavora automaticamente nella cartella in cui si trova questo file BAT
pushd "%~dp0" >nul 2>&1

echo ============================================================
echo      OPENNEOUA MOTORE - MANUTENZIONE GIT SICURA
echo ============================================================
echo.
echo Cartella:
echo %CD%
echo.

rem Verifica che Git sia disponibile
where git >nul 2>&1
if errorlevel 1 (
    echo ERRORE: Git non e' disponibile nel PATH.
    echo Installa o configura Git per Windows e riprova.
    echo.
    pause
    popd
    exit /b 1
)

rem Verifica che la cartella sia un repository Git
git rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 (
    echo ERRORE: questa cartella non contiene un repository Git valido.
    echo Metti questo file BAT nella cartella principale di OpenNeoUA,
    echo cioe' quella che contiene la cartella nascosta .git
    echo.
    pause
    popd
    exit /b 1
)

echo Repository rilevato:
git rev-parse --show-toplevel
echo.

echo Stato Git attuale:
git status --short
echo.

echo Dimensioni PRIMA della manutenzione:
git count-objects -vH
echo.

echo Questa operazione eseguira' soltanto:
echo     git gc
echo.
echo Non modifica il sorgente.
echo Non riscrive la cronologia.
echo Non esegue prune aggressivi.
echo.

choice /C SN /N /M "Procedere? [S/N]: "
if errorlevel 2 (
    echo.
    echo Operazione annullata.
    echo.
    pause
    popd
    exit /b 0
)

echo.
echo Manutenzione Git in corso...
echo Non chiudere questa finestra.
echo.

git gc
if errorlevel 1 (
    echo.
    echo ERRORE: git gc non e' terminato correttamente.
    echo Nessun comando aggressivo e' stato eseguito.
    echo.
    pause
    popd
    exit /b 1
)

echo.
echo Dimensioni DOPO la manutenzione:
git count-objects -vH
echo.

echo Stato Git finale:
git status --short
echo.

echo ============================================================
echo Manutenzione OpenNeoUA completata con successo.
echo ============================================================
echo.
pause

popd
endlocal
