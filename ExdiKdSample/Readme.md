This project is modified EXDI plugins example from WinDBG SDK
(C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\sdk\samples\exdi\ExdiKdSample)

It uses Hyper-V memory manager library for reading and writing memory operations and other operations with Hyper-V CPU

There is readme.docx in ExdiKdSample project in WinDBG SDK directory, which describes architecture of EXDI interface. If shortly,
EXDI is an interface, that allows extending WinDBG by adding support for hardware debuggers. We didn't have hardware debuggers for Hyper-V, but we can add hvlib functions calling to EXDI interface.

![](./images/EXDi.png)

For installation: 

1. Copy LiveCloudKd.exe, hvlib.dll, hvmm.sys and ExdiKdSample.dll to WinDBG x64 directory
2. Register ExdiKdSample.dll using regsvr32.exe ExdiKdSample.dll command
3. Start LiveCloudKd with /w option: LiveCloudKd.exe /w. It is automatically launch WinDBG with EXDI interface.

![](./images/EXDi2.png)

You can use WinDBG or WinDBG (Classic) with EXDI plugin

1. Launch LiveCloudKd.exe /x.
2. Copy string, which provided by LiveCloudKd. 

![](./images/EXDi3.png)

3. Start WinDBG, go to File-Start debugging-Attach to Kernel, open EXDI tab and paste string to field or execute

```
DbgX.Shell.exe -v -kx exdi:CLSID={53838F70-0936-44A9-AB4E-ABB568401508},Kd=Guess
```

![](./images/EXDi4.png)

4. You can read\write to guest OS memory and execute WinDBG commands, that work for dump files. Remember, that it is static plugin example, and you can't do single step (F11, F8) or running commands (F5).

![](./images/EXDi5.png)

Detailed information about EXDi live debugging you can find on [this page](LiveDebugging.md)
