@echo off
sc create VerifierTest type= kernel start= demand error= normal binPath= System32\Drivers\VerifierTest.sys DisplayName= VerifierTest
