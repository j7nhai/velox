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

class VeloxExecDemo : public VectorTestBase {
 public:
  const std::string kTpchConnectorId = "test-tpch";

  VeloxExecDemo() {
    // Register Presto scalar functions.
    functions::prestosql::registerAllScalarFunctions();

    // Register Presto aggregate functions.
    aggregate::prestosql::registerAllAggregateFunctions();

    // Register type resolver with DuckDB SQL parser.
    parse::registerTypeResolver();

    // Register the TPC-H Connector Factory.
    connector::registerConnectorFactory(
        std::make_shared<connector::tpch::TpchConnectorFactory>());

    // Create and register a TPC-H connector.
    auto tpchConnector =
        connector::getConnectorFactory(
            connector::tpch::TpchConnectorFactory::kTpchConnectorName)
            ->newConnector(
                kTpchConnectorId,
                std::make_shared<config::ConfigBase>(
                    std::unordered_map<std::string, std::string>()));
    connector::registerConnector(tpchConnector);
  }

  ~VeloxExecDemo() {
    connector::unregisterConnector(kTpchConnectorId);
    connector::unregisterConnectorFactory(
        connector::tpch::TpchConnectorFactory::kTpchConnectorName);
  }

  /// Parse SQL expression into a typed expression tree using DuckDB SQL parser.
  core::TypedExprPtr parseExpression(
      const std::string& text,
      const RowTypePtr& rowType) {
    parse::ParseOptions options;
    auto untyped = parse::parseExpr(text, options);
    return core::Expressions::inferTypes(untyped, rowType, execCtx_->pool());
  }

  /// Compile typed expression tree into an executable ExprSet.
  std::unique_ptr<exec::ExprSet> compileExpression(
      const std::string& expr,
      const RowTypePtr& rowType) {
    std::vector<core::TypedExprPtr> expressions = {
        parseExpression(expr, rowType)};
    return std::make_unique<exec::ExprSet>(
        std::move(expressions), execCtx_.get());
  }

  /// Evaluate an expression on one batch of data.
  VectorPtr evaluate(exec::ExprSet& exprSet, const RowVectorPtr& input) {
    exec::EvalCtx context(execCtx_.get(), &exprSet, input.get());

    SelectivityVector rows(input->size());
    std::vector<VectorPtr> result(1);
    exprSet.eval(rows, context, result);
    return result[0];
  }

  /// Make TPC-H split to add to TableScan node.
  exec::Split makeTpchSplit() const {
    return exec::Split(
        std::make_shared<connector::tpch::TpchConnectorSplit>(
            kTpchConnectorId, /*cacheable=*/true, 1, 0));
  }

  /// Run the demo.
  void run();

  std::shared_ptr<folly::Executor> executor_{
      std::make_shared<folly::CPUThreadPoolExecutor>(
          std::thread::hardware_concurrency())};
  std::shared_ptr<core::QueryCtx> queryCtx_{
      core::QueryCtx::create(executor_.get())};
  std::unique_ptr<core::ExecCtx> execCtx_{
      std::make_unique<core::ExecCtx>(pool_.get(), queryCtx_.get())};
};

void VeloxExecDemo::run() {
  auto planNodeIdGenerator = std::make_shared<core::PlanNodeIdGenerator>();
  core::PlanNodeId orderScanId;
  auto plan = PlanBuilder(planNodeIdGenerator)
                  .tpchTableScan(
                      tpch::Table::TBL_ORDERS,
                      {"o_custkey", "o_totalprice"},
                      1 /*scaleFactor*/)
                  .capturePlanNodeId(orderScanId)
                  .singleAggregation({"o_custkey"}, {"count(*) as totalprice"})
                  .orderBy({"totalprice desc"}, false)
                  .planNode();

  auto result = AssertQueryBuilder(plan)
                    .split(orderScanId, makeTpchSplit())
                    .copyResults(pool());
  std::cout << "show stack " << std::endl
            << process::StackTrace().toString() << std::endl;
  std::cout << plan->toString(true, true) << std::endl;
  std::cout << std::endl
            << "> number of nations per region in TPC-H: " << result->toString()
            << std::endl;
  std::cout << result->toString(0, 10) << std::endl;
}

int main(int argc, char** argv) {
  FLAGS_v = 0;
  FLAGS_minloglevel = 0;
  FLAGS_logtostderr = true;
  FLAGS_velox_exception_user_stacktrace_enabled = true;

#if __linux__
  LOG(WARNING) << "is Linux";
#endif

#if FOLLY_HAVE_ELF
  LOG(WARNING) << "folly have elf";
#endif

#if FOLLY_HAVE_DWARF
  LOG(WARNING) << "folly have dwarf";
#endif

  folly::Init init{&argc, &argv, false};

  // Initializes the process-wide memory-manager with the default options.
  memory::initializeMemoryManager({});

  VeloxExecDemo demo;
  demo.run();
}
