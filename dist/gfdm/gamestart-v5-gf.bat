@echo off

cd /d %~dp0

inject gfdmhook1.dll gdv5.exe -g --config gfdm-v5-gf.conf %*
