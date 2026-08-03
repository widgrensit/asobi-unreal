#include "AsobiCore/Auth.h"

#include "AsobiCore/Json.h"

namespace asobi::core
{

namespace
{

AuthError MakeError(int StatusCode, std::string_view Body)
{
    AuthError Out;
    Out.StatusCode = StatusCode;

    const auto Reason = json::TopLevelString(Body, "error");
    if (Reason && !Reason->empty())
    {
        Out.Reason = *Reason;
    }
    else
    {
        Out.Reason = std::string(StatusCode == 0 ? kAuthErrorNetwork : kAuthErrorUnknown);
    }
    return Out;
}

} // namespace

AuthResult ParseAuthResponse(int StatusCode, std::string_view Body)
{
    AuthResult Out;

    const bool bHttpOk = StatusCode >= 200 && StatusCode < 300;
    if (!bHttpOk)
    {
        Out.Error = MakeError(StatusCode, Body);
        return Out;
    }

    const auto Access = json::TopLevelString(Body, "access_token");
    if (!Access || Access->empty())
    {
        Out.Error.StatusCode = StatusCode;
        Out.Error.Reason = std::string(kAuthErrorMalformedResponse);
        return Out;
    }

    Out.Success = true;
    Out.Tokens.AccessToken = *Access;
    if (const auto V = json::TopLevelString(Body, "refresh_token"))
    {
        Out.Tokens.RefreshToken = *V;
    }
    if (const auto V = json::TopLevelString(Body, "player_id"))
    {
        Out.Tokens.PlayerId = *V;
    }
    if (const auto V = json::TopLevelString(Body, "username"))
    {
        Out.Tokens.Username = *V;
    }
    if (const auto V = json::TopLevelBool(Body, "created"))
    {
        Out.Tokens.Created = *V;
    }
    if (const auto V = json::TopLevelBool(Body, "guest"))
    {
        Out.Tokens.Guest = *V;
    }
    if (const auto V = json::TopLevelBool(Body, "upgraded"))
    {
        Out.Tokens.Upgraded = *V;
    }
    return Out;
}

} // namespace asobi::core
