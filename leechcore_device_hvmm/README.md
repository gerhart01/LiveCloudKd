This is MemProcFS plugin for reading Hyper-V memory using Hyper-V Memory Manager library

Sources was taken from https://github.com/ufrisk/LeechCore

- MemProcFs can be found on https://github.com/ufrisk/MemProcFS by @ulfrisk

- LiveCloudKd: https://github.com/gerhart01/LiveCloudKd

Copy leechcore_device_hvmm.dll with hvlib.dll and hvmm.sys to MemProcFS folder

start MemProcFS:
```
MemProcFS.exe -device hvmm -v
```

you must see something like that:

![](./images/1.png)

Next you can go to M: driver and use pypykatz plugin, f.e.

![](./images/2.png)

## `mapped`: reads through a persistent mapping

`hvmm://id=1,mapped` asks `hvmm.sys` to map the guest's physical memory into the
MemProcFS process once, read-only, and serves every scatter read from that mapping
instead of one driver round trip per page. Needs an `hvmm.sys` with
`IOCTL_MAP_GPA_RANGE`; older drivers answer the handshake with an error and the plugin
falls back to the driver path on its own. Full VMs only; containers keep the driver
path. Set `LC_HVMM_MAPPED=1` in the environment to turn it on for a program whose
device string you cannot change. If the VM stops, the next failed read drops the
mapping and the driver path takes over.

## `keepdriver`: leave the driver loaded

By default the plugin stops and deletes the `hvmm` service when the last LeechCore
context in the process closes. That is fine for one tool at a time, but when several
programs read the same VM it takes `hvmm.sys` away from all of them, and a program
that opens and closes often reloads the driver over and over.

`hvmm://id=1,mapped,keepdriver` closes the device handle and the mapping as usual but
leaves the service alone: the driver is loaded on first use and stays loaded until the
machine reboots. `LC_HVMM_KEEP_DRIVER=1` in the environment does the same for a program
whose device string you cannot change.

Because the driver now outlives the program, the plugin also checks which file an
existing `hvmm` service points at. A service keeps the driver file it was created with,
so an old service pointing somewhere else would silently load a different `hvmm.sys`.
If the paths differ and the service is stopped, the plugin deletes it and creates it
again with the `hvmm.sys` next to `leechcore.dll`, printing one line about it. If the
paths differ and the service is running, it stops and names both files — stop that
driver yourself first.
