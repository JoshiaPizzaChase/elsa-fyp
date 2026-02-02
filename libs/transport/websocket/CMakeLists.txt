add_library(websocket_lib INTERFACE)

target_include_directories(websocket_lib INTERFACE
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_sources(MyHeaderOnlyLib INTERFACE
        # List your actual header files here
        websocket.h
        websocket_client.h
        websocket_server.h
)
