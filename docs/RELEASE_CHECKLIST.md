# Release checklist

Releases are cut automatically: merging to `main` runs `.github/workflows/release.yml`,
which bumps the version from the conventional-commit history, tags it and publishes a
GitHub release. Everything below is what a maintainer verifies around that.

## Coverage boundary (decision, 2026-08-03)

CI on hosted runners covers the protocol and the backend contract, but **not** the
`Asobi.Smoke` UE Automation spec. Running that spec needs an installed Unreal Engine
(multi-GB, licence-gated) plus a host UE project, which a hosted `ubuntu-24.04` runner
cannot provide.

The call for now is **accept the partial CI coverage and run `Asobi.Smoke` manually
around each release**, rather than standing up a self-hosted UE runner or paying for a
UE-image action. Rationale:

- The dispatch surface (35 canonical server events) is fully gated on every PR by the
  engine-agnostic `asobi-core` job, which is where drift bugs actually appear.
- The wire contract (`POST /api/v1/auth/register`, `/ws` upgrade) is gated on every PR
  against a real `sdk_demo_backend` container.
- What is left uncovered is the UE glue layer (`UAsobiWebSocket`, `UAsobiClient` HTTP),
  which changes rarely and is exercised by the manual run plus the demo project.
- A self-hosted runner carries ongoing cost: engine upgrades, disk, security exposure of
  a persistent runner on a public repo.

Revisit this call if any of these become true:

- A UE glue-layer regression ships to a release (the manual gate has failed in practice).
- The plugin gains a second engine-side transport or an auto-reconnect layer, i.e. the
  uncovered surface stops being thin.
- A self-hosted UE machine already exists for another repo, so the marginal cost is only
  the runner registration.

## Per release

1. `main` is green: `Smoke` workflow (`asobi-core`, `smoke-source-syntax`,
   `backend-contract`) and `Lint`.
2. Run the `Asobi.Smoke` UE Automation spec manually against a live backend:

   ```bash
   git clone https://github.com/widgrensit/sdk_demo_backend
   (cd sdk_demo_backend && docker compose up -d)

   UE5Editor-Cmd /path/to/MyProject.uproject \
     -ExecCmds="Automation RunTests Asobi.Smoke; Quit" \
     -unattended -nullrhi -log
   ```

   Pass criteria: both players authenticate and connect, both receive `match.matched`
   with the same `match_id`, and a `match.input` `{move_x:1}` moves the sending player's
   `x` past `x_initial + 10`. Details and troubleshooting live in
   [`Source/AsobiSDK/Tests/SmokeTest.md`](../Source/AsobiSDK/Tests/SmokeTest.md).

3. Record the result as a comment on the published release (engine version used, backend
   commit, pass/fail). A release with no such comment is a release nobody smoke-tested.
4. If the run fails, ship the fix before announcing the release; the tag is already
   public, so the correction is a follow-up patch release, not a retag.

## Per plugin-version bump

- `AsobiSDK.uplugin` `EngineVersion` still matches the minimum UE version the code
  requires (currently UE 5.4+).
- README feature table and quick-start snippets still compile against the shipped API.
