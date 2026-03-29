if (MSVC)
    target_compile_options(modules PRIVATE /bigobj)
endif ()

target_include_directories(modules PUBLIC "mod-i-found-your-sword/libs")
target_include_directories(modules PUBLIC "mod-i-found-your-sword/libs/wswrap")
target_include_directories(modules PUBLIC "mod-i-found-your-sword/libs/asio")
