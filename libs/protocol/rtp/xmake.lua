target("protocol_rtp", function () 
    set_kind("static")
    add_files("**.cpp")
    add_includedirs(".", { public = true })
    add_deps("base")
    -- add_deps("codec_h264", "codec_hevc")
end)

