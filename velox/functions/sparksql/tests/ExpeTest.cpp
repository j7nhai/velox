/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <gmock/gmock.h>
#include <optional>

#include "velox/common/base/tests/GTestUtils.h"
#include "velox/functions/sparksql/tests/SparkFunctionBaseTest.h"

namespace facebook::velox::functions::sparksql::test {
using namespace facebook::velox::test;

namespace {

class ExpeTest : public SparkFunctionBaseTest {
 protected:
  std::optional<std::string> trim(const std::optional<std::string>& a) {
    return evaluateOnce<std::string>("expe_trim(c0)", a);
  }

  template <typename T>
  std::optional<T> greatest(
      std::optional<T> arg0,
      std::optional<T> arg1,
      std::optional<T> arg2,
      const TypePtr& type = CppToType<T>::create()) {
    return evaluateOnce<T>(
        "expe_greatest(c0, c1, c2)", {type, type, type}, arg0, arg1, arg2);
  }
};

TEST_F(ExpeTest, trim) {
  EXPECT_EQ(trim("okokok"), "okokok");
  EXPECT_EQ(trim("  okokok"), "okokok");
  EXPECT_EQ(trim("okokok  "), "okokok");
  EXPECT_EQ(trim("  okokok  "), "okokok");
  EXPECT_EQ(trim("      "), "");
  EXPECT_EQ(trim(""), "");
  EXPECT_EQ(trim("  a    "), "a");
}

TEST_F(ExpeTest, greatest) {
  EXPECT_EQ(greatest<int32_t>(3, 6, -1), 6);
  EXPECT_EQ(greatest<int32_t>(13, 6, -1), 13);
}

} // namespace
} // namespace facebook::velox::functions::sparksql::test
