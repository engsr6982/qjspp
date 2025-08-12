add_rules("mode.debug", "mode.release")

if has_config("test") then
    add_requires("catch2 v3.8.1")
end

if is_plat("windows") then
    if not has_config("vs_runtime") then
        -- set_runtimes("MD")
        if is_mode("debug") then 
            set_runtimes("MDd")
        else 
            set_runtimes("MD")
        end 
    end
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

    if is_mode("debug") then
        add_defines("QJSPP_DEBUG")
    end

    if has_config("qjs_include") then
        add_includedirs(get_config("qjs_include"))
    end

    if not has_config("test") then 
        set_kind("static")
    else 
        set_kind("binary")
        add_files("tests/**.cc")
        add_packages("catch2")
        add_links(get_config("qjs_lib"))
        print("DEBUG: qjs_include: "..get_config("qjs_include"))
        print("DEBUG: qjs_lib: "..get_config("qjs_lib"))
    end

    after_build(function (target)
        local binDir = os.projectdir() .. "/bin"
        if not os.isdir(binDir) then
            os.mkdir(binDir)
        end
        local test = target:targetfile()
        os.cp(test, binDir)
    end)
target_end()
