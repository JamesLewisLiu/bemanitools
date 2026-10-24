@echo off

cd /d %~dp0

inject gfdmhook1.dll gdv8.exe -g --config gfdm-v8-gf.conf %*
