@echo off

cd /d %~dp0

inject gfdmhook1.dll gdv8.exe -d --config gfdm-v8-dm.conf %*
