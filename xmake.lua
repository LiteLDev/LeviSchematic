add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")
add_repositories("miracleforest-repo https://github.com/MiracleForest/xmake-repo.git")

add_requires("levilamina 26.60.*", {configs = {target_type = get_config("target_type")}})

add_requires("levibuildscript")

option("target_type")
    set_default("client")
    set_showmenu(true)
    set_values("client")
option_end()

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("LeviSchematic")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    if is_plat("windows") then
        add_syslinks("user32", "version", "d3dcompiler")
        add_defines("NOMINMAX", "UNICODE")
        set_exceptions("none")
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
