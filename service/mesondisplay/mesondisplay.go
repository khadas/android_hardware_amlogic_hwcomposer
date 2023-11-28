package hwc_mesondisplay

import (
    "android/soong/android"
    "android/soong/cc"
    //"fmt"
    "strconv"
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
   // fmt.Println("PlatformVndkVersion:", PlatformVndkVersion)
    //For Android T, before freeze API PlatformVndkVersion return string like "Tiramisu", after freeze API it will be changed to be numbers like normal android release "32"
    IntPlatformVndkVersion,err := strconv.Atoi(PlatformVndkVersion)
    if err != nil {
        //fmt.Printf("%v fail to convert", PlatformVndkVersion)
        p.Whole_static_libs = append(p.Static_libs, "libjsoncpp")
    } else {
        //fmt.Println("IntPlatformVndkVersion:", IntPlatformVndkVersion)
        if IntPlatformVndkVersion > 31 {
            p.Whole_static_libs = append(p.Static_libs, "libjsoncpp")
        } else {
            p.Static_libs = append(p.Static_libs, "libjsoncpp")
        }
    }

    ctx.AppendProperties(p)
}
