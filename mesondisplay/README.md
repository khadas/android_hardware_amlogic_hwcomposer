# introduc
the hwc used api implement on display_adapter_local, systemcontrol and other process use a
display client which under display_adapter_remote
architecture like below:
## Android mode:
          HWC--->DisplayServer--->DisplayAdapterLocal(shared library)
                       ||
                       ||(HDIL Binder)
                       ||
                   DisplayClient <-----|
systemcontrol--> DisplayAdapterRemot-->|

## Android Recovery mode:
systemcontrol--> DisplayAdapterLocal(static library).

