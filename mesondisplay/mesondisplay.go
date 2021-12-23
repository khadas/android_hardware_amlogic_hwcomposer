package hwc_mesondisplay

import (
    "android/soong/android"
    "android/soong/cc"
)

func init() {
    android.RegisterModuleType("hwc_mesondisplay_go_defaults",mesondisplay_DefaultsFactory)
}

func mesondisplay_DefaultsFactory() (android.Module) {
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, hwc_mesondisplay_aml_Defaults)
    return module
}

func hwc_mesondisplay_aml_Defaults(ctx android.LoadHookContext) {
    type propsE struct {
        Static_libs  []string
        Whole_static_libs  []string
    }
    p := &propsE{}
    PlatformVndkVersion := ctx.DeviceConfig().PlatformVndkVersion()
    //fmt.Println("PlatformVndkVersion:", PlatformVndkVersion)
    //fmt.Println("len(PlatformVndkVersion:", len(PlatformVndkVersion)
    // After Andriod T, PlatformVndkVersion return string like "Tiramisu", not string number like "32"
    if len(PlatformVndkVersion) > 2 {
        p.Whole_static_libs = append(p.Static_libs, "libjsoncpp")
    } else {
        p.Static_libs = append(p.Static_libs, "libjsoncpp")
    }
    ctx.AppendProperties(p)
}
