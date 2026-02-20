#pragma once

#include <filesystem>
#include <iostream>

#include "quickfix/FileStore.h"
#include "quickfix/SessionSettings.h"
#include "quickfix/SocketAcceptor.h"

#include <catch2/catch_test_macros.hpp>

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/roles/client_endpoint.hpp>