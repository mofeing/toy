add_rules("mode.debug", "mode.release")
set_languages("c++20")

add_requires("brew::llvm", {alias = "llvm", configs = {shared = true}})

target("toy")
    set_kind("shared")
    add_files(
        "src/lib.cppm",
        "src/generator.cppm",
        "src/lexer.cppm",
        "src/parser.cppm",
        "src/dumper.cppm"
        )
    add_packages("llvm", {public=true})
    add_links("c++")
    add_links("termcap", {public=true})

target("toy-dump")
    set_kind("binary")
    add_files("src/bin/toy-dump.cpp")
    add_deps("toy")
