@echo off

cd /d %~dp0

inject gfdmhook1.dll gdv4.exe -d --config gfdm-v4-dm.conf %*
