// Thin wrapper that pulls AsobiCore's engine-agnostic protocol code into
// the UE module so UnrealBuildTool compiles it without any per-platform
// prebuilt or CMake step. AsobiCore depends only on plain C++17 — no
// UE types — so this is safe.
//
// See Source/AsobiCore/CMakeLists.txt and AsobiSDK.Build.cs for context.

#include "../../AsobiCore/Private/Auth.cpp"
#include "../../AsobiCore/Private/Json.cpp"
#include "../../AsobiCore/Private/Protocol.cpp"
