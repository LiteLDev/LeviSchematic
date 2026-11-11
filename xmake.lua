add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

add_requires("levilamina 26.60.4", {configs = {target_type = get_config("target_type")}})

add_requires("levibuildscript")

option("target_type")
    set_default("client")
    set_showmenu(true)
    set_values("client")
option_end()

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

option("target_type")
    set_default("client")
    set_showmenu(true)
    set_values("client")
option_end()

target("LeviSchematic") -- Change this to your mod name.
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    if is_plat("windows") then
        add_syslinks("user32", "version") -- Input focus and version-gated empty SDK render types.
        add_defines("NOMINMAX", "UNICODE")
        set_exceptions("none") -- To avoid conflicts with /EHa.
        add_cxflags( "/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
        add_cxflags(
            "/EHs",
            "-Wno-microsoft-cast",
            "-Wno-invalid-offsetof",
            "-Wno-c++2b-extensions",
            "-Wno-microsoft-include",
            "-Wno-overloaded-virtual",
            "-Wno-ignored-qualifiers",
            "-Wno-missing-field-initializers",
            "-Wno-potentially-evaluated-expression",
            "-Wno-pragma-system-header-outside-header",
            {tools = {"clang_cl"}}
        )
        set_toolchains("clang-cl")
    end
    add_packages("levilamina")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    add_headerfiles(
        "src/**.h"
    )
    add_files("src/**.cpp")
    add_includedirs("src")
    after_build(function (target)
        os.mkdir("bin/LeviSchematic/assets")
        os.cp("assets/ui", "bin/LeviSchematic/assets/")
    end)
    if is_config("target_type", "server") then
    --  add_includedirs("src-server")
    --  add_files("src-server/**.cpp")
    else
    --  add_includedirs("src-client")
    --  add_files("src-client/**.cpp")
    end

-- 独立回归目标：显式构建/运行，不改变正式 DLL 的完整源码范围。
for _, name in ipairs({"SessionGateTests", "ChunkCpuTaskTests", "SchematicRenderAlgorithmsTests", "NativeTextureVariantTests", "RefactorStateTests", "ProjectionStateTests", "UiModelTests"}) do
    target(name)
        set_default(false)
        set_kind("binary")
        set_languages("c++20")
        set_toolchains("clang-cl")
        add_cxflags("/EHsc", "/utf-8", "/UNDEBUG")
        add_defines("NOMINMAX", "UNICODE")
        add_includedirs("src")
        add_packages("levilamina")
        add_files("src-test/" .. name .. ".cpp")
        if name == "UiModelTests" then
            add_files("src/levischematic/ui/Ui.cpp")
            add_files("src/levischematic/ui/UiLayout.cpp")
        end
        if name == "ProjectionStateTests" then
            add_files("src/levischematic/schematic/placement/PlacementStore.cpp")
        end
end
