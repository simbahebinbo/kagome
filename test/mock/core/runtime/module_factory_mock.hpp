/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "runtime/module_factory.hpp"

#include <gmock/gmock.h>

#include "runtime/module.hpp"

namespace kagome::runtime {

  class ModuleFactoryMock final : public ModuleFactory {
   public:
    MOCK_METHOD((CompilationOutcome<std::shared_ptr<Module>>),
                make,
                (common::BufferView),
                (const, override));
  };
}  // namespace kagome::runtime
