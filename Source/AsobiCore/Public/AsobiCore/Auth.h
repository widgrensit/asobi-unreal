// AsobiCore — engine-agnostic parsing of the Asobi auth HTTP responses.
//
// Both the success shape (token pair + guest flags) and the failure shape
// (`{"error":"<reason>"}` alongside the HTTP status) are mapped here so the
// mapping can be unit-tested headlessly on stock CI. The UE-side UAsobiAuth
// is a pure marshalling shim over this: it converts FString <-> std::string
// and copies the result into the Blueprint-facing USTRUCTs.
//
// Mirrors asobi's asobi_auth_controller / asobi_guest_controller error
// envelope: every non-2xx auth response carries a stable snake_case `error`
// string (weak_device_secret, guest_capacity_reached, device_already_
// registered, guest_revoked, username_taken, validation_failed, ...).

#pragma once

#include <string>
#include <string_view>

namespace asobi::core
{

struct AuthTokens
{
    std::string AccessToken;
    std::string RefreshToken;
    std::string PlayerId;
    std::string Username;
    bool Created = false;
    bool Guest = false;
    bool Upgraded = false;
};

struct AuthError
{
    // HTTP status, or 0 when the request never got a response.
    int StatusCode = 0;
    std::string Reason;
};

struct AuthResult
{
    bool Success = false;
    AuthTokens Tokens;
    AuthError Error;
};

// The request never reached the backend (DNS/connect/TLS failure, timeout).
inline constexpr std::string_view kAuthErrorNetwork = "network_error";

// The backend answered 2xx but the body carried no usable access token.
inline constexpr std::string_view kAuthErrorMalformedResponse = "malformed_response";

// The backend failed but the body carried no `error` field (proxy error
// page, empty body, non-JSON).
inline constexpr std::string_view kAuthErrorUnknown = "unknown_error";

// Maps a raw auth HTTP response to the SDK-facing result. Pass StatusCode 0
// with an empty body when the request never got a response.
AuthResult ParseAuthResponse(int StatusCode, std::string_view Body);

} // namespace asobi::core
