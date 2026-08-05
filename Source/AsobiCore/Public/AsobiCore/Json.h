// AsobiCore — minimal top-level JSON field reader.
//
// The Asobi wire protocol only ever needs a handful of top-level scalar
// fields out of an envelope (`type`, `error`, `access_token`, ...). Pulling
// in a full JSON library for that would force every consumer to vendor one,
// so AsobiCore ships this narrow reader instead: it walks the top-level
// object, skips over nested values, and returns the first matching key.
//
// Deliberately NOT a general-purpose JSON parser. Values are returned as text
// or as a raw slice; numbers and \uXXXX decoding are out of scope — the UE-side
// SDK still uses FJsonObject for the rich payload types.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace asobi::core::json
{

// Reads a top-level string field. Returns nullopt when the JSON is
// malformed, the key is absent, or the value is not a string.
std::optional<std::string> TopLevelString(std::string_view Json, std::string_view Key);

// Reads a top-level boolean field. Returns nullopt when the JSON is
// malformed, the key is absent, or the value is not `true`/`false`.
std::optional<bool> TopLevelBool(std::string_view Json, std::string_view Key);

// Reads the RAW JSON value at a nested key path, quotes and braces included:
// RawPath(R"({"payload":{"result":{"reward":100}}})", {"payload", "result"})
// gives {"reward":100}, and {"cid"} on a reply gives "c-1" WITH its quotes.
//
// Raw rather than decoded because the two callers need it that way: RPC
// results and error details are defined by the extension rather than by this
// protocol, so they are handed to the game as JSON instead of being forced
// through a shape we guessed at; and cid correlation only ever compares a
// token to the one it sent, so a server echoing a string and one echoing a
// number both match without agreeing a type first.
//
// Returns nullopt if the JSON is malformed or any step of the path is absent.
std::optional<std::string> RawPath(std::string_view Json, const std::vector<std::string>& Path);

// Unwraps a raw JSON string token to its text. Returns nullopt for anything
// that is not a string, so a caller can tell an absent field from one holding
// a number or an object.
std::optional<std::string> Unquote(std::string_view Raw);

} // namespace asobi::core::json
