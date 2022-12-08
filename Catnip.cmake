
catnip_package(calico)

catnip_add_preset(gba_release TOOLSET GBA BUILD_TYPE Release)

catnip_add_preset(ds7_release TOOLSET NDS PROCESSOR armv4t  BUILD_TYPE MinSizeRel)
catnip_add_preset(ds9_release TOOLSET NDS PROCESSOR armv5te BUILD_TYPE Release)
