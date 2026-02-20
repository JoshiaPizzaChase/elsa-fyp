#pragma once

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "quickfix/Application.h"
#include "quickfix/Field.h"
#include "quickfix/FileStore.h"
#include "quickfix/Session.h"
#include "quickfix/SocketInitiator.h"
#include "quickfix/fix42/ExecutionReport.h"
#include "quickfix/fix42/MessageCracker.h"
#include "quickfix/fix42/NewOrderSingle.h"
#include "quickfix/fix42/OrderCancelReject.h"
#include "quickfix/fix42/OrderCancelRequest.h"

#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"