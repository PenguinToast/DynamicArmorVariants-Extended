set_xmakever("3.0.0")

includes("lib/commonlibsse-ng")

set_project("DynamicArmorVariants")
set_version("1.0.5")
set_license("MIT")

set_languages("c++23")
set_warnings("allextra")
set_policy("package.requires_lock", true)

set_config("skse_xbyak", true)

add_rules("mode.release", "mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

add_requires("jsoncpp 1.9.6", "ordered_map 1.1.0")

target("DynamicArmorVariants")
    set_kind("shared")

    add_deps("commonlibsse-ng")
    add_packages("jsoncpp", "ordered_map")

    add_rules("commonlibsse-ng.plugin", {
        name = "DynamicArmorVariants",
        author = "Parapets",
        description = "Dynamic armor variant framework ported to CommonLibSSE-NG",
        options = {
            address_library = true
        }
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src", "src/main", "src/PCH")
    set_pcxxheader("src/PCH/PCH.h")
