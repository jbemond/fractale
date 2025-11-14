@echo off
set X=256
set Y=192
set SEED=42
set SEA=0.45

geo.exe -x %X% -y %Y% -s %SEED% -a 1 -k 0.65 -f 2 --sea %SEA% --from-edge -o map.ppm
if errorlevel 1 ( echo Erreur geo.exe & exit /b 1 )

start "" "%CD%\ppm_viewer.html"
echo Ouvre la page et glisse le fichier map.ppm dedans.