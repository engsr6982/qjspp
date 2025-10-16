add_rules("mode.debug", "mode.release")

-- v0.10.1
add_requires("quickjs-ng 3c9afc9943323ee9c7dbd123c0cd991448f4b6c2", { configs={ libc=true } })

if has_config("test") then
    add_requires("catch2 v3.8.1")
end

if is_plat("windows") then
    if not has_config("vs_runtime") then 
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


target("qjspp")
    add_files("src/**.cc")
    add_includedirs("src", "include")
    add_headerfiles("include/(qjspp/**.hpp)", "include/(qjspp/**.inl)")
    set_languages("cxx20")
    set_symbols("debug")
    add_packages("quickjs-ng")

    if is_plat("windows") then 
        add_cxflags("/utf-8", "/W4", "/sdl")
    elseif is_plat("linux") then
        add_cxflags("-fPIC", "-stdlib=libc++", {force = true})
        add_syslinks("dl", "pthread")
    end

    if is_mode("debug") then
        add_defines("QJSPP_DEBUG")
    end

    if has_config("test") then
        set_kind("binary")
        add_files("tests/**.cc")
        add_packages("catch2")
    else
        set_kind(get_config("kind") or "static")
        if is_kind("shared") then
            add_defines("QJSPP_SHARED")
            if is_plat("windows") then
                add_defines("QJSPP_EXPORTS")
            end
        end
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
