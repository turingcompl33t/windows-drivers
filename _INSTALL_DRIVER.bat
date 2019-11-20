:: InstallDriver.bat
:: Install a kernel mode driver.

sc create <SERVICE NAME> binpath= <PATH TO .sys> type= kernel