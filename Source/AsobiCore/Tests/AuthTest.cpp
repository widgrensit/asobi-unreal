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

TEST_CASE("backend guest error codes survive to the caller")
{
    struct Case
    {
        int StatusCode;
        const char* Body;
        const char* Reason;
    };

    // Mirrors asobi_guest_controller / asobi_auth_controller.
    const Case Cases[] = {
        {400, R"({"error":"weak_device_secret"})",      "weak_device_secret"},
        {400, R"({"error":"invalid_device_id"})",       "invalid_device_id"},
        {400, R"({"error":"missing_required_fields"})", "missing_required_fields"},
        {401, R"({"error":"guest_revoked"})",           "guest_revoked"},
        {401, R"({"error":"invalid_device_secret"})",   "invalid_device_secret"},
        {401, R"({"error":"invalid_credentials"})",     "invalid_credentials"},
        {403, R"({"error":"guest_auth_disabled"})",     "guest_auth_disabled"},
        {409, R"({"error":"device_already_registered"})", "device_already_registered"},
        {409, R"({"error":"username_taken"})",          "username_taken"},
        {409, R"({"error":"not_an_unclaimed_guest"})",  "not_an_unclaimed_guest"},
        {503, R"({"error":"guest_capacity_reached"})",  "guest_capacity_reached"},
    };

    for (const Case& C : Cases)
    {
        CAPTURE(C.Reason);
        const AuthResult R = ParseAuthResponse(C.StatusCode, C.Body);
        CHECK_FALSE(R.Success);
        CHECK(R.Error.StatusCode == C.StatusCode);
        CHECK(R.Error.Reason == C.Reason);
        CHECK(R.Tokens.AccessToken.empty());
    }
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
