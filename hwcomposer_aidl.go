package hw_composer_aidl

import (
    "android/soong/android"
    "android/soong/cc"
    //"fmt"
)

func init() {
    android.RegisterModuleType("hw_composer_aidl_go_defaults",composer_aidl_DefaultsFactory)
}

func composer_aidl_DefaultsFactory() (android.Module) {
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, hw_composer_aidl_Defaults)
    return module
}

func hw_composer_aidl_Defaults(ctx android.LoadHookContext) {
    type propsE struct {
        Shared_libs  []string
    }
    p := &propsE{}

    PlatformSdkExtensionVersion := ctx.AConfig().PlatformSdkExtensionVersion()
    //fmt.Println(" hwcomposer PlatformSdkExtensionVersion:",PlatformSdkExtensionVersion);

    PlatformSDKVersion:= ctx.AConfig().PlatformSdkVersion();
    //fmt.Println(" hwcomposer PlatformSdkExtensionVersion:",PlatformSDKVersion);

    // For An Android letter, before freeze API PlatformVndkVersion return code name, like
    // "UpsideDownCake", after freeze API it has been changed to number.
    if PlatformSDKVersion.FinalOrFutureInt() > 33 ||
            (PlatformSDKVersion.FinalOrFutureInt() == 33 && PlatformSdkExtensionVersion >= 5) {
        p.Shared_libs = append(p.Shared_libs, "android.hardware.graphics.composer3-V2-ndk")
    } else {
        p.Shared_libs = append(p.Shared_libs, "android.hardware.graphics.composer3-V1-ndk")
    }
    ctx.AppendProperties(p)
}
