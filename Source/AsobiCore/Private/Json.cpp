#include "AsobiCore/Json.h"

#include <cstddef>

namespace asobi::core::json
{

namespace
{

// Skips whitespace, returns new index.
std::size_t SkipWs(std::string_view S, std::size_t I)
{
    while (I < S.size())
    {
        const char C = S[I];
        if (C == ' ' || C == '\t' || C == '\n' || C == '\r')
        {
            ++I;
        }
        else
        {
            break;
        }
    }
    return I;
}

// Reads a JSON string starting at S[I] (must point at the opening quote).
// Returns the unescaped string and advances I past the closing quote. On
// malformed input returns nullopt and leaves I in an unspecified state.
std::optional<std::string> ReadString(std::string_view S, std::size_t& I)
{
    if (I >= S.size() || S[I] != '"')
    {
        return std::nullopt;
    }
    ++I;
    std::string Out;
    while (I < S.size())
    {
        const char C = S[I];
        if (C == '"')
        {
            ++I;
            return Out;
        }
        if (C == '\\')
        {
            if (I + 1 >= S.size())
            {
                return std::nullopt;
            }
            const char Esc = S[I + 1];
            switch (Esc)
            {
            case '"':  Out.push_back('"');  break;
            case '\\': Out.push_back('\\'); break;
            case '/':  Out.push_back('/');  break;
            case 'b':  Out.push_back('\b'); break;
            case 'f':  Out.push_back('\f'); break;
            case 'n':  Out.push_back('\n'); break;
            case 'r':  Out.push_back('\r'); break;
            case 't':  Out.push_back('\t'); break;
            case 'u':
                // Asobi envelopes do not use \uXXXX in the fields this
                // reader is pointed at, but consume the 4 hex digits to
                // stay tolerant. We intentionally do not decode the
                // codepoint — anyone who ships a non-ASCII codepoint in a
                // protocol identifier can wire up a real JSON lib.
                if (I + 5 >= S.size())
                {
                    return std::nullopt;
                }
                Out.append(S.substr(I + 2, 4));
                I += 4;
                break;
            default:
                return std::nullopt;
            }
            I += 2;
        }
        else
        {
            Out.push_back(C);
            ++I;
        }
    }
    return std::nullopt;
}

// Skips a JSON value at S[I]. Used to walk past values we do not care
// about while looking for the wanted key. Recurses for nested
// objects/arrays.
bool SkipValue(std::string_view S, std::size_t& I);

bool SkipObject(std::string_view S, std::size_t& I)
{
    if (I >= S.size() || S[I] != '{')
    {
        return false;
    }
    ++I;
    I = SkipWs(S, I);
    if (I < S.size() && S[I] == '}')
    {
        ++I;
        return true;
    }
    while (I < S.size())
    {
        I = SkipWs(S, I);
        auto Key = ReadString(S, I);
        if (!Key)
        {
            return false;
        }
        I = SkipWs(S, I);
        if (I >= S.size() || S[I] != ':')
        {
            return false;
        }
        ++I;
        I = SkipWs(S, I);
        if (!SkipValue(S, I))
        {
            return false;
        }
        I = SkipWs(S, I);
        if (I < S.size() && S[I] == ',')
        {
            ++I;
            continue;
        }
        if (I < S.size() && S[I] == '}')
        {
            ++I;
            return true;
        }
        return false;
    }
    return false;
}

bool SkipArray(std::string_view S, std::size_t& I)
{
    if (I >= S.size() || S[I] != '[')
    {
        return false;
    }
    ++I;
    I = SkipWs(S, I);
    if (I < S.size() && S[I] == ']')
    {
        ++I;
        return true;
    }
    while (I < S.size())
    {
        I = SkipWs(S, I);
        if (!SkipValue(S, I))
        {
            return false;
        }
        I = SkipWs(S, I);
        if (I < S.size() && S[I] == ',')
        {
            ++I;
            continue;
        }
        if (I < S.size() && S[I] == ']')
        {
            ++I;
            return true;
        }
        return false;
    }
    return false;
}

bool SkipValue(std::string_view S, std::size_t& I)
{
    I = SkipWs(S, I);
    if (I >= S.size())
    {
        return false;
    }
    const char C = S[I];
    if (C == '{')
    {
        return SkipObject(S, I);
    }
    if (C == '[')
    {
        return SkipArray(S, I);
    }
    if (C == '"')
    {
        auto V = ReadString(S, I);
        return V.has_value();
    }
    if (C == 't' || C == 'f' || C == 'n')
    {
        // true / false / null
        while (I < S.size())
        {
            const char Ch = S[I];
            if ((Ch >= 'a' && Ch <= 'z'))
            {
                ++I;
            }
            else
            {
                break;
            }
        }
        return true;
    }
    // number — consume digits, sign, exponent, decimal
    if (C == '-' || C == '+' || (C >= '0' && C <= '9'))
    {
        while (I < S.size())
        {
            const char Ch = S[I];
            if ((Ch >= '0' && Ch <= '9') || Ch == '.' || Ch == '-' || Ch == '+'
                || Ch == 'e' || Ch == 'E')
            {
                ++I;
            }
            else
            {
                break;
            }
        }
        return true;
    }
    return false;
}

// Walks the top-level object looking for Key. On success leaves I at the
// first character of that key's value.
bool SeekTopLevelValue(std::string_view S, std::string_view Key, std::size_t& I)
{
    I = SkipWs(S, 0);
    if (I >= S.size() || S[I] != '{')
    {
        return false;
    }
    ++I;
    while (I < S.size())
    {
        I = SkipWs(S, I);
        if (I < S.size() && S[I] == '}')
        {
            return false;
        }
        auto Found = ReadString(S, I);
        if (!Found)
        {
            return false;
        }
        I = SkipWs(S, I);
        if (I >= S.size() || S[I] != ':')
        {
            return false;
        }
        ++I;
        I = SkipWs(S, I);
        if (*Found == Key)
        {
            return true;
        }
        if (!SkipValue(S, I))
        {
            return false;
        }
        I = SkipWs(S, I);
        if (I < S.size() && S[I] == ',')
        {
            ++I;
            continue;
        }
        return false;
    }
    return false;
}

bool StartsWith(std::string_view S, std::size_t I, std::string_view Literal)
{
    return I <= S.size() && S.size() - I >= Literal.size()
        && S.compare(I, Literal.size(), Literal) == 0;
}

} // namespace

std::optional<std::string> TopLevelString(std::string_view Json, std::string_view Key)
{
    std::size_t I = 0;
    if (!SeekTopLevelValue(Json, Key, I))
    {
        return std::nullopt;
    }
    return ReadString(Json, I);
}

std::optional<bool> TopLevelBool(std::string_view Json, std::string_view Key)
{
    std::size_t I = 0;
    if (!SeekTopLevelValue(Json, Key, I))
    {
        return std::nullopt;
    }
    if (StartsWith(Json, I, "true"))
    {
        return true;
    }
    if (StartsWith(Json, I, "false"))
    {
        return false;
    }
    return std::nullopt;
}

} // namespace asobi::core::json
