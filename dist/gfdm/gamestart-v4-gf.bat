@echo off

cd /d %~dp0

inject gfdmhook1.dll gdv4.exe -g --config gfdm-v4-gf.conf %*
