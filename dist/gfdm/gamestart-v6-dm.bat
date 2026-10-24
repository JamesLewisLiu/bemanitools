@echo off

cd /d %~dp0

inject gfdmhook1.dll gdv6.exe -d --config gfdm-v6-dm.conf %*
