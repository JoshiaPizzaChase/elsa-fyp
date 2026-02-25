#pragma once

#include <optional>
#include <stdexcept>
#include <string>

#include <quickfix/Application.h>
#include <quickfix/Except.h>
#include <quickfix/FixCommonFields.h>
#include <quickfix/FixFields.h>
#include <quickfix/FixValues.h>
#include <quickfix/MessageCracker.h>
#include <quickfix/fix42/ExecutionReport.h>
#include <quickfix/fix42/NewOrderSingle.h>
#include <quickfix/fix42/OrderCancelRequest.h>

#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"