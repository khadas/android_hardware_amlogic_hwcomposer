package hwc_mesondisplay

import (
	"android/soong/android"
	"android/soong/cc"
	//"fmt"
)

func init() {
	android.RegisterModuleType("hwc_mesondisplay_go_defaults", mesondisplay_DefaultsFactory)
}

func mesondisplay_DefaultsFactory() android.Module {
	module := cc.DefaultsFactory()
	android.AddLoadHook(module, hwc_mesondisplay_aml_Defaults)
	return module
}

func hwc_mesondisplay_aml_Defaults(ctx android.LoadHookContext) {
	type propsE struct {
		Static_libs       []string
		Whole_static_libs []string
	}
	p := &propsE{}

	p.Whole_static_libs = append(p.Static_libs, "libjsoncpp")

	ctx.AppendProperties(p)
}
