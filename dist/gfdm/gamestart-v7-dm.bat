@echo off

cd /d %~dp0

inject gfdmhook1.dll gdv7.exe -d --config gfdm-v7-dm.conf %*
