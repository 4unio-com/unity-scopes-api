if(CLICK_MODE)
    if(NOT BZR_REVNO)
        set(BZR_REVNO "latest")
    endif(NOT BZR_REVNO)
    configure_file(
        manifest.json.in
        "${CMAKE_CURRENT_BINARY_DIR}/manifest.json"
    )
    install(
        FILES
            "scope-security.json"
            "${CMAKE_CURRENT_BINARY_DIR}/manifest.json"
        DESTINATION
            ${CMAKE_INSTALL_PREFIX}
    )
endif(CLICK_MODE)
