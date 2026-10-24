@echo off

cd /d %~dp0

inject gfdmhook1.dll gdv7.exe -g --config gfdm-v7-gf.conf %*
