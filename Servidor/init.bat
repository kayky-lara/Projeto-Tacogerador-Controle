@echo off

:: Obter o diretório atual
set "current_dir=%cd%"

:: Iniciar o primeiro processo em um novo shell
start cmd /k "cd /d %current_dir%\BackED\BackED && dotnet run BackED.cprojs"

:: Iniciar o segundo processo em um novo shell
start brave.exe %current_dir%\index.html