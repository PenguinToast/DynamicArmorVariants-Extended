set_xmakever("3.0.0")

includes("lib/commonlibsse-ng")

local build_version = os.getenv("DAVE_BUILD_VERSION") or "1.1.0"
local build_version_string = os.getenv("DAVE_BUILD_VERSION_STRING") or build_version
local major, minor, patch = build_version:match("^(%d+)%.(%d+)%.(%d+)$")
if not major then
    error("DAVE_BUILD_VERSION must be in major.minor.patch format, got " .. build_version)
end

set_project("DynamicArmorVariantsExtended")
set_version(build_version)
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
    set_basename("DynamicArmorVariants")

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

    add_defines("DAVE_VERSION_MAJOR=" .. major)
    add_defines("DAVE_VERSION_MINOR=" .. minor)
    add_defines("DAVE_VERSION_PATCH=" .. patch)
    add_defines('DAVE_VERSION_STRING="' .. build_version_string .. '"')

    add_files("src/**.cpp")
    add_headerfiles("src/**.h", "include/**.h")
    add_includedirs("src", "src/main", "src/PCH", "include")
    set_pcxxheader("src/PCH/PCH.h")
