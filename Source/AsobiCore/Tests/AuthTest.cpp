// Auth response-mapping unit test for AsobiCore.
//
// Covers the seam widgrensit/asobi-unreal#12 asked for: every auth
// entrypoint (register / login / refresh / oauth / guest / guest upgrade)
// routes its HTTP response through ParseAuthResponse, so the guest error
// codes the backend returns must survive to the caller as a status + reason
// instead of collapsing to a bare `false`.
//
// Runs on stock CI (ubuntu-24.04 + g++/clang + doctest). No Unreal Engine
// install required.
//
// Build & run:
//   cmake -S Source/AsobiCore -B build
//   cmake --build build
//   cd build && ctest --output-on-failure

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "AsobiCore/Auth.h"

#include <string>

using namespace asobi::core;

TEST_CASE("guest create parses tokens and flags")
{
    const std::string Body = R"({"access_token":"at","refresh_token":"rt",)"
                             R"("player_id":"pid","username":"guest_7","created":true,)"
                             R"("guest":true,"upgraded":false})";

    const AuthResult R = ParseAuthResponse(200, Body);

    CHECK(R.Success);
    CHECK(R.Tokens.AccessToken == "at");
    CHECK(R.Tokens.RefreshToken == "rt");
    CHECK(R.Tokens.PlayerId == "pid");
    CHECK(R.Tokens.Username == "guest_7");
    CHECK(R.Tokens.Created);
    CHECK(R.Tokens.Guest);
    CHECK_FALSE(R.Tokens.Upgraded);
    CHECK(R.Error.StatusCode == 0);
    CHECK(R.Error.Reason.empty());
}

TEST_CASE("guest resume reports created=false")
{
    const AuthResult R = ParseAuthResponse(
        200, R"({"access_token":"at","player_id":"pid","created":false,"guest":true})");

    CHECK(R.Success);
    CHECK_FALSE(R.Tokens.Created);
    CHECK(R.Tokens.Guest);
}

TEST_CASE("absent flags default to false")
{
    const AuthResult R = ParseAuthResponse(200, R"({"access_token":"at"})");

    CHECK(R.Success);
    CHECK_FALSE(R.Tokens.Created);
    CHECK_FALSE(R.Tokens.Guest);
    CHECK_FALSE(R.Tokens.Upgraded);
    CHECK(R.Tokens.RefreshToken.empty());
}

TEST_CASE("guest upgrade reports upgraded=true")
{
    const AuthResult R = ParseAuthResponse(
        200, R"({"access_token":"at","username":"claimed","guest":false,"upgraded":true})");

    CHECK(R.Success);
    CHECK(R.Tokens.Upgraded);
    CHECK_FALSE(R.Tokens.Guest);
    CHECK(R.Tokens.Username == "claimed");
}

TEST_CASE("2xx without an access token is a failure, not a silent empty success")
{
    const AuthResult R = ParseAuthResponse(200, R"({"player_id":"pid"})");

    CHECK_FALSE(R.Success);
    CHECK(R.Error.StatusCode == 200);
    CHECK(R.Error.Reason == kAuthErrorMalformedResponse);
}

TEST_CASE("2xx with an empty access token is a failure")
{
    const AuthResult R = ParseAuthResponse(200, R"({"access_token":""})");

    CHECK_FALSE(R.Success);
    CHECK(R.Error.Reason == kAuthErrorMalformedResponse);
}

TEST_CASE("backend error codes survive to the caller")
{
    struct Case
    {
        int StatusCode;
        const char* Body;
        const char* Code;
        const char* Reason;
    };

    // The shared error object, which is what every current asobi answers with.
    // These fixtures used to carry the pre-object flat names
    // ("guest_capacity_reached", "not_an_unclaimed_guest") that the server
    // stopped sending, so the suite was green against a contract nobody spoke.
    const Case Cases[] = {
        {400, R"({"error":{"code":"guest.weak_device_secret","message":"Too short.","details":{}}})",
         "guest.weak_device_secret", "Too short."},
        {401, R"({"error":{"code":"guest.invalid_device_secret","message":"No match.","details":{}}})",
         "guest.invalid_device_secret", "No match."},
        {403, R"({"error":{"code":"guest.disabled","message":"Off.","details":{}}})",
         "guest.disabled", "Off."},
        {403, R"({"error":{"code":"player.confirmation_failed","message":"Wrong.","details":{}}})",
         "player.confirmation_failed", "Wrong."},
        {409, R"({"error":{"code":"guest.not_unclaimed","message":"Claimed.","details":{}}})",
         "guest.not_unclaimed", "Claimed."},
        {429, R"({"error":{"code":"guest.rate_limited","message":"Slow down.","details":{"retry_after":5}}})",
         "guest.rate_limited", "Slow down."},
        {503, R"({"error":{"code":"guest.capacity_reached","message":"Full.","details":{}}})",
         "guest.capacity_reached", "Full."},
        {503, R"({"error":{"code":"guest.unavailable","message":"Cannot count.","details":{}}})",
         "guest.unavailable", "Cannot count."},
    };

    for (const Case& C : Cases)
    {
        CAPTURE(C.Code);
        const AuthResult R = ParseAuthResponse(C.StatusCode, C.Body);
        CHECK_FALSE(R.Success);
        CHECK(R.Error.StatusCode == C.StatusCode);
        CHECK(R.Error.Code == C.Code);
        CHECK(R.Error.Reason == C.Reason);
    }
}

TEST_CASE("a flat legacy error body still reaches the caller")
{
    // Some routes kept a flat body, and an older deployment may send one, so
    // dropping this path would break against a server that is merely behind.
    const AuthResult R = ParseAuthResponse(403, R"({"error":"guest_auth_disabled"})");
    CHECK_FALSE(R.Success);
    CHECK(R.Error.Reason == "guest_auth_disabled");
    CHECK(R.Error.Code.empty());
}

TEST_CASE("an error object with no message still yields its code")
{
    const AuthResult R = ParseAuthResponse(500, R"({"error":{"code":"internal","details":{}}})");
    CHECK(R.Error.Code == "internal");
    CHECK(R.Error.Reason == kAuthErrorUnknown);
}

TEST_CASE("validation_failed keeps its reason past the nested fields object")
{
    const AuthResult R = ParseAuthResponse(
        422, R"({"fields":{"password":"too_short","error":"decoy"},"error":"validation_failed"})");

    CHECK_FALSE(R.Success);
    CHECK(R.Error.StatusCode == 422);
    CHECK(R.Error.Reason == "validation_failed");
}

TEST_CASE("no response at all maps to network_error")
{
    const AuthResult R = ParseAuthResponse(0, "");

    CHECK_FALSE(R.Success);
    CHECK(R.Error.StatusCode == 0);
    CHECK(R.Error.Reason == kAuthErrorNetwork);
}

TEST_CASE("non-JSON or error-less failure bodies map to unknown_error")
{
    CHECK(ParseAuthResponse(502, "<html>502 Bad Gateway</html>").Error.Reason == kAuthErrorUnknown);
    CHECK(ParseAuthResponse(500, "").Error.Reason == kAuthErrorUnknown);
    CHECK(ParseAuthResponse(500, R"({"message":"boom"})").Error.Reason == kAuthErrorUnknown);
    CHECK(ParseAuthResponse(500, R"({"error":""})").Error.Reason == kAuthErrorUnknown);
    CHECK(ParseAuthResponse(500, R"({"error":"")").Error.Reason == kAuthErrorUnknown);
}

TEST_CASE("whitespace and escapes are tolerated")
{
    const AuthResult Ok = ParseAuthResponse(201, "{ \n \"access_token\" : \"a\\/b\" , \"guest\" : true }");
    CHECK(Ok.Success);
    CHECK(Ok.Tokens.AccessToken == "a/b");
    CHECK(Ok.Tokens.Guest);

    const AuthResult Err = ParseAuthResponse(409, "  {  \"error\"  :  \"username_taken\"  }  ");
    CHECK(Err.Error.Reason == "username_taken");
}
