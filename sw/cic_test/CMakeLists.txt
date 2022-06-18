if (TARGET tinyusb_device)
    add_executable(cic_test
            cic_test.c
            cic.c
            )

    # pull in common dependencies
    target_link_libraries(
        cic_test
        pico_multicore
        pico_stdlib
    )

    # enable usb output, disable uart output
    pico_enable_stdio_usb(cic_test 0)
    pico_enable_stdio_uart(cic_test 0)

    # create map/bin/hex/uf2 file etc.
    pico_add_extra_outputs(cic_test)

    # add url via pico_set_program_url
    example_auto_set_url(cic_test)
elseif(PICO_ON_DEVICE)
    message(WARNING "not building cic_test because TinyUSB submodule is not initialized in the SDK")
endif()
