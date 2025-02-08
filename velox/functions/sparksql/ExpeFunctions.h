#pragma once
#pragma once

#include "folly/ssl/OpenSSLHash.h"

#include <codecvt>
#include <string>
#include "velox/expression/VectorFunction.h"
#include "velox/functions/Macros.h"
#include "velox/functions/UDFOutputString.h"
#include "velox/functions/lib/string/StringCore.h"
#include "velox/functions/lib/string/StringImpl.h"

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
    result.setEmpty();
    // result.setNoCopy(new StringView(srcStr.data(), srcStr.size()));
  }
};

template <typename T>
struct ExpeTrimFunction : public ExpeTrimSpaceFunctionBase<T> {};
} // namespace facebook::velox::functions::sparksql
