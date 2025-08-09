add_rules("mode.debug", "mode.release")
set_project("qjspp")
set_version("0.1.0")

if has_config("test") then
    add_requires("catch2 v3.8.1")
end

if is_plat("windows") then
    if not has_config("vs_runtime") then
        set_runtimes("MD")
    end
elseif is_plat("linux") then 
    set_toolchains("clang")
end

option("test")
    set_default(false)
    set_showmenu(true)
option_end()

option("qjs_include")
    set_default("")
    set_showmenu(true)
    set_description("Path to quickjs include directory (e.g., /usr/local/include/quickjs)")
option_end()

option("qjs_lib")
    set_default("")
    set_showmenu(true)
    set_description("Path to quickjs static library (e.g., /usr/local/lib/quickjs.a)")
option_end()


target("qjspp")
    set_kind("static")
    add_files("src/**.cc")
    add_includedirs("src", "include")
    add_headerfiles("include/(qjspp/**.hpp)")
    set_languages("cxx20")
    set_symbols("debug")

    if is_plat("windows") then 
        add_cxflags("/utf-8", "/W4", "/sdl")
    elseif is_plat("linux") then
        add_cxflags("-fPIC", "-stdlib=libc++", {force = true})
        add_syslinks("dl", "pthread")
    end

    if has_config("qjs_include") then
        add_includedirs(get_config("qjs_include"))
    end

    if is_mode("debug") then
        add_defines("QJSPP_DEBUG")
    end
target_end()


target("qjspp_test")
    add_deps("qjspp")
    set_kind("binary")
    set_symbols("debug")
    set_languages("cxx20")
    add_includedirs("include") -- add qjspp include dir
    add_files("tests/**.cc")
    set_symbols("debug")

    if is_plat("linux") then
        add_packages("catch2", {links = {"Catch2", "Catch2Main"}})
    else
        add_packages("catch2")
    end

    if is_plat("windows") then 
        add_cxflags("/utf-8", "/W4", "/sdl")
    elseif is_plat("linux") then
        add_cxflags("-fPIC", "-stdlib=libc++", {force = true})
        add_ldflags("-stdlib=libc++", {force = true})
        add_syslinks("dl", "pthread", "c++", "m")
    end

    add_includedirs(get_config("qjs_include"))
    add_links(get_config("qjs_lib"))

    after_build(function (target)
        local binDir = os.projectdir() .. "/bin"
        if not os.isdir(binDir) then
            os.mkdir(binDir)
        end
        local test = target:targetfile()
        os.cp(test, binDir)
    end)
