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
#include "velox/expression/SpecialFormRegistry.h"
#include "velox/functions/lib/Re2Functions.h"
#include "velox/functions/prestosql/StringFunctions.h"

#include "velox/functions/sparksql/ExpeFunctions.h"

namespace facebook::velox::functions {

namespace sparksql {
void registerStringFunctions(const std::string& prefix) {
  registerFunction<ExpeTrimFunction, Varchar, Varchar>({prefix + "expe_trim"});
}
} // namespace sparksql
} // namespace facebook::velox::functions
