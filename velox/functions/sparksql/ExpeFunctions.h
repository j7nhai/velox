#pragma once

#include "folly/ssl/OpenSSLHash.h"

#include <codecvt>
#include <string>
#include "velox/core/CoreTypeSystem.h"
#include "velox/expression/VectorFunction.h"
#include "velox/functions/Macros.h"
#include "velox/functions/UDFOutputString.h"
#include "velox/functions/lib/string/StringCore.h"
#include "velox/functions/lib/string/StringImpl.h"
#include "velox/type/StringView.h"

namespace facebook::velox::functions::sparksql {

template <typename T>
struct ExpeTrimSpaceFunctionBase {
  VELOX_DEFINE_FUNCTION_TYPES(T);

  // Results refer to strings in the first argument.
  static constexpr int32_t reuse_strings_from_arg = 0;

  // ASCII input always produces ASCII result.
  static constexpr bool is_default_ascii_behavior = true;

  FOLLY_ALWAYS_INLINE void call(
      out_type<Varchar>& result,
      const arg_type<Varchar>& srcStr) {
    if (srcStr.empty()) {
      result.setEmpty();
      return;
    }
    auto data = srcStr.data();
    auto begin = data;
    auto end = begin + srcStr.size();
    while (begin < end && *begin == ' ') {
      begin++;
    }
    while (begin < end && *(end - 1) == ' ') {
      end--;
    }
    result.setNoCopy(StringView(begin, end - begin));
  }
};

template <typename T>
struct ExpeTrimFunction : public ExpeTrimSpaceFunctionBase<T> {};

class ExpeGreatestFunction final : public exec::VectorFunction {
  void apply(
      const SelectivityVector& rows,
      std::vector<VectorPtr>& args,
      const TypePtr& outputType,
      exec::EvalCtx& context,
      VectorPtr& result) const override {
    const size_t nargs = args.size();
    const auto nrows = rows.end();

    // Setup result vector.
    context.ensureWritable(rows, outputType, result);
    FlatVector<int32_t>& flatResult = *result->as<FlatVector<int32_t>>();

    // NULL all elements.
    rows.applyToSelected(
        [&](vector_size_t row) { flatResult.setNull(row, true); });

    exec::LocalSelectivityVector cmpRows(context, nrows);
    exec::LocalDecodedVector decodedVectorHolder(context);
    // Column-wise process: one argument at a time.
    for (size_t i = 0; i < nargs; i++) {
      decodedVectorHolder.get()->decode(*args[i], rows);

      // Only compare with non-null elements of each argument
      *cmpRows = rows;
      if (auto* rawNulls = decodedVectorHolder->nulls(&rows)) {
        cmpRows->deselectNulls(rawNulls, 0, nrows);
      }

      cmpRows->applyToSelected([&](vector_size_t row) {
        auto srcVector = args[i]->as<FlatVector<int32_t>>();
        if (!srcVector->isNullAt(row)) {
          auto element = srcVector->valueAt(row);
          if (flatResult.isNullAt(row)) {
            flatResult.set(row, element);
          } else {
            auto value = flatResult.valueAt(row);
            if (element > value) {
              flatResult.set(row, element);
            }
          }
        }
      });
    }
  }
};

std::shared_ptr<exec::VectorFunction> expeMakeGreatest(
    const std::string& functionName,
    const std::vector<exec::VectorFunctionArg>& args,
    const core::QueryConfig& /*config*/) {
  return std::make_shared<ExpeGreatestFunction>();
}

std::vector<std::shared_ptr<exec::FunctionSignature>> expeGreaterSignatures() {
  std::vector<std::string> types = {"integer"};
  std::vector<std::shared_ptr<exec::FunctionSignature>> signatures;

  for (const auto& type : types) {
    signatures.emplace_back(exec::FunctionSignatureBuilder()
                                .returnType(type)
                                .argumentType(type)
                                .variableArity(type)
                                .build());
  }
  return signatures;
}

} // namespace facebook::velox::functions::sparksql
