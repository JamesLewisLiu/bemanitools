@echo off

cd /d %~dp0

inject gfdmhook1.dll gdv6.exe -g --config gfdm-v6-gf.conf %*
