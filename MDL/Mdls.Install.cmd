@echo off
sc create Mdls type= kernel start= demand error= normal binPath= System32\Drivers\Mdls.sys DisplayName= Mdls
