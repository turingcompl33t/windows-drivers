## File System Minifilters

### Loading & Unloading a Minifilter Driver

Use the `fltmc` tool to load a minifilter driver. 

```
fltmc.exe load MyFilterDriver
```

Similiarly, use `fltmc` to unload a minifilter driver.

```
fltmc.exe unload MyFilterDriver
```

### Installing a Minifilter Driver

The proper way to install a minifilter driver is via an INF file.

Once the INF file has been defined, one may simply click on it in Explorer to install the driver. The driver may subsequently be loaded via the `fltmc` tool as described above.