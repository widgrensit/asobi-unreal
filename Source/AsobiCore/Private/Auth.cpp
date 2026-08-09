#include "AsobiCore/Auth.h"

#include "AsobiCore/Json.h"

namespace asobi::core
{

namespace
{

// asobi answers every failure with a shared error object,
// {"error": {"code": ..., "message": ..., "details": {...}}}. TopLevelString
// returns nullopt for an object value, so against any current server this fell
// straight through to kAuthErrorUnknown and both halves were lost - a wrong
// password and a rate limit arrived identical. The flat legacy form,
// {"error": "some_string"}, is still read: a few routes kept it and an older
// deployment may send one.
AuthError MakeError(int StatusCode, std::string_view Body)
{
    AuthError Out;
    Out.StatusCode = StatusCode;

    if (const auto Object = json::RawPath(Body, {"error"}); Object && !Object->empty() && (*Object)[0] == '{')
    {
        if (const auto Code = json::TopLevelString(*Object, "code"))
        {
            Out.Code = *Code;
        }
        if (const auto Message = json::TopLevelString(*Object, "message"); Message && !Message->empty())
        {
            Out.Reason = *Message;
            return Out;
        }
    }
    else if (const auto Flat = json::TopLevelString(Body, "error"); Flat && !Flat->empty())
    {
        Out.Reason = *Flat;
        return Out;
    }

    Out.Reason = std::string(StatusCode == 0 ? kAuthErrorNetwork : kAuthErrorUnknown);
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
