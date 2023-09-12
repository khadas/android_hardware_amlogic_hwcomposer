# introduce
the hwc used api implement on display_adapter_local, systemcontrol and other process use a
display client which under display_adapter_remote
architecture like below:
## Android mode:
                   DisplayServer--->DisplayAdapterLocal(shared library)--->HWC
                       ||
                       ||(HDIL Binder)
                       ||
                   DisplayClient <------|
systemcontrol--> DisplayAdapterRemote-->|

## Android Recovery mode:
systemcontrol--> DisplayAdapterLocal(static library).

