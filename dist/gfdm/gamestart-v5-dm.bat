@echo off

cd /d %~dp0

inject gfdmhook1.dll gdv5.exe -d --config gfdm-v5-dm.conf %*
