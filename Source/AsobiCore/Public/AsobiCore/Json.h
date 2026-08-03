// AsobiCore — minimal top-level JSON field reader.
//
// The Asobi wire protocol only ever needs a handful of top-level scalar
// fields out of an envelope (`type`, `error`, `access_token`, ...). Pulling
// in a full JSON library for that would force every consumer to vendor one,
// so AsobiCore ships this narrow reader instead: it walks the top-level
// object, skips over nested values, and returns the first matching key.
//
// Deliberately NOT a general-purpose JSON parser. Nested lookups, arrays,
// numbers and \uXXXX decoding are out of scope — the UE-side SDK still uses
// FJsonObject for the rich payload types.

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace asobi::core::json
{

// Reads a top-level string field. Returns nullopt when the JSON is
// malformed, the key is absent, or the value is not a string.
std::optional<std::string> TopLevelString(std::string_view Json, std::string_view Key);

// Reads a top-level boolean field. Returns nullopt when the JSON is
// malformed, the key is absent, or the value is not `true`/`false`.
std::optional<bool> TopLevelBool(std::string_view Json, std::string_view Key);

} // namespace asobi::core::json
