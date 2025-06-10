target("mms-rtp", function () 
    set_kind("static")
    add_files("**.cpp")
    add_includedirs(".", { public = true })
    add_deps("mms-base")
end)

