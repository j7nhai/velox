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
#include <folly/init/Init.h>
#include "velox/common/memory/Memory.h"
#include "velox/connectors/tpch/TpchConnector.h"
#include "velox/connectors/tpch/TpchConnectorSplit.h"
#include "velox/core/Expressions.h"
#include "velox/exec/tests/utils/AssertQueryBuilder.h"
#include "velox/exec/tests/utils/PlanBuilder.h"
#include "velox/expression/Expr.h"
#include "velox/functions/prestosql/aggregates/RegisterAggregateFunctions.h"
#include "velox/functions/prestosql/registration/RegistrationFunctions.h"
#include "velox/parse/Expressions.h"
#include "velox/parse/ExpressionsParser.h"
#include "velox/parse/TypeResolver.h"
#include "velox/tpch/gen/TpchGen.h"
#include "velox/vector/tests/utils/VectorTestBase.h"

using namespace facebook::velox;
using namespace facebook::velox::test;
using namespace facebook::velox::exec::test;

class MyVeloxDemo : public VectorTestBase {
public:
  /**
   * this demo show how to build a vector with values 0 ... 99.
   */
  void constructIntVector() const {
    std::cout << std::endl << "=== demo: constructIntVector ===" << std::endl;
    constexpr int n = 100;
    auto values = AlignedBuffer::allocate<int32_t>(100, pool(), 0);
    auto* rawValues = values->asMutable<int32_t>();
    for (int i = 0; i < n; i++) {
      rawValues[i] = i;
    }
    auto result = std::make_shared<FlatVector<int32_t>> (
      pool(),
      INTEGER(),
      nullptr,
      n,
      values,
      std::vector<BufferPtr>{}
    );
    std::cout << result->toString(0, result->size()) << std::endl;
  }

  void constructIndexArrayVector() const {
    std::cout << std::endl << "=== demo: constructIndexArrayVector ===" << std::endl;
    constexpr int numElement = 6;
    const vector_size_t sizes[numElement] = {3, 2, 5, 3, 1, 2};
    const vector_size_t numFlattenElement = std::accumulate(sizes, sizes + numElement, 0);
    auto result = constructIndexArrayVector(numElement, numFlattenElement, sizes);
    std::cout << result->toString(0, result->size()) << std::endl;
  }

private:
  ArrayVectorPtr constructIndexArrayVector(const int numElement, const int numFlattenElement, const vector_size_t* sizes) const {
    BufferPtr resultOffsets = AlignedBuffer::allocate<vector_size_t>(
            numElement, pool(), 0);
    auto mutableOffsets = resultOffsets->asMutable<vector_size_t>();

    BufferPtr resultSizes = AlignedBuffer::allocate<vector_size_t>(
      numElement, pool(), 0);
    auto mutableSizes = resultSizes->asMutable<vector_size_t>();

    mutableSizes[0] = sizes[0];
    for (int i = 1; i < numElement; i++) {
      mutableOffsets[i] = sizes[i - 1] + mutableOffsets[i - 1];
      mutableSizes[i] = sizes[i];
    }

    auto elements = makeIndexFlattenElementVector(pool(), numElement, numFlattenElement, sizes);
    auto nulls = AlignedBuffer::allocate<bool>(numElement, pool(), bits::kNotNull);
    auto result = std::make_shared<ArrayVector>(
      pool(),
      ARRAY(INTEGER()),
      nullptr,
      numElement,
      std::move(resultOffsets),
      std::move(resultSizes),
      std::move(elements)
    );
    return result;
  }

  static FlatVectorPtr<int32_t> makeIndexFlattenElementVector(
  memory::MemoryPool* pool,
  const vector_size_t numElement,
  const vector_size_t numFlattenElement,
  const vector_size_t* sizes) {

    auto values = AlignedBuffer::allocate<int32_t>(numFlattenElement, pool, 0);
    auto* rawValues = values->asMutable<int32_t>();

    int idx = 0;
    for (int i = 0; i < numElement; i++) {
      vector_size_t sizeOfRow = sizes[i];
      for (int j = 0; j < sizeOfRow; j++) {
        rawValues[idx++] = j;
      }
    }

    return std::make_shared<FlatVector<int32_t>> (
      pool,
      INTEGER(),
      nullptr,
      idx,
      values,
      std::vector<BufferPtr>{}
    );
  }
};

int main(int argc, char** argv) {
  folly::Init init{&argc, &argv, false};

  // Initializes the process-wide memory-manager with the default options.
  memory::initializeMemoryManager({});

  MyVeloxDemo myDemo;
  myDemo.constructIntVector();
  myDemo.constructIndexArrayVector();

  return 0;
}
