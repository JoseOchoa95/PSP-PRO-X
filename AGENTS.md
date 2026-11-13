# PRO-X — Agent Instructions

PRO-X is a PSP custom firmware project based on PRO-C. Implement only the requested feature or phase, preserve existing work, and distinguish implemented functionality from functionality validated on physical hardware.

## Project context and repository layout

Before starting or resuming work:

1. Read the root `README.md` for the general project description and current public status.
2. Read `context/README.md` for the index of previous implementations and their status.
3. Read the relevant implementation records and validation results in `context/`.
4. Read the requested feature or phase specification in `workflow/`.
5. Review the current source, Git state, and applicable nested instructions before making changes.

Do not assume another chat's history is available. Reconstruct the current state from the repository and its recorded evidence.

| Location | Purpose |
| --- | --- |
| `README.md` | General, public-facing project description, capabilities, and status. |
| `workflow/` | Specifications of work to implement: objectives, scope, requirements, and acceptance criteria. |
| `context/README.md` | Index of completed or ongoing implementation records and their current status. |
| `context/` | What was actually done: implementation details, decisions, test results, evidence, limitations, and pending work. |
| `CHANGELOG.md` | User-facing changes prepared for the next release. |
| `builds/` | Final compiled firmware and test packages delivered to the user before commit/PR authorization; ignored by Git. |
| `artifacts/` | Local build logs, intermediate trees, verification output and raw diagnostic evidence; ignored by Git. |

This project does not use `docs/`. Keep implementation and validation documentation in `context/`, and plans in `workflow/`.

Legacy files currently under `docs/` include licenses, pinned upstream inventories and build-toolchain inputs referenced by existing scripts. Preserve these dependencies during artifact cleanup; migrating their references is a separate change. Do not delete them as generated output.

Do not replace a workflow specification with its implementation report. Do not record completed work in `workflow/` as a substitute for maintaining `context/`.

## Required outputs after each feature or phase

Complete the documentation and release notes as part of the implementation, before requesting authorization for Git publication.

### Implementation record

Create or update `context/<workflow-specification-filename>.md`, using the same basename as the corresponding specification.

For example:

- `workflow/phase-1-installable-pro-x.md` → `context/phase-1-installable-pro-x.md`
- `workflow/phase-2-native-wpa2.md` → `context/phase-2-native-wpa2.md`

If there is no corresponding workflow file, use a descriptive implementation-record filename and add it to the context index.

Include:

- Feature or phase, date, and actual implementation/validation status.
- Relevant source revision, product version, and build identity when known.
- Work performed and important implementation decisions.
- Changes or deviations from the requested specification.
- Checks and tests actually executed, with results and available evidence.
- Generated artifacts and their identity or hashes when applicable.
- Failures, limitations, blockers, pending tests, and next steps.

Preserve useful history and unresolved issues. Update the record at meaningful checkpoints even if the work is unfinished or blocked.

### Context README

Create `context/README.md` if missing; otherwise update its index with a relative link to the implementation record and a concise, accurate status.

Preserve existing entries. The index must let a new chat find the relevant prior work without reading the full conversation history.

### Root README

After every feature, review the root `README.md` and determine whether its general description, feature list, requirements, or public status needs updating.

Update it when the change affects what the project offers or what users should understand about it, such as the addition of an available feature or a meaningful status change.

Keep it general. Detailed implementation steps, logs, test matrices, and phase history belong in `context/`.

If the feature does not affect the general project description or status, leave the root README unchanged and state that it was reviewed and did not need changes.

### Release changelog

For every completed feature:

- Create or update the root `CHANGELOG.md`.
- Use an `Unreleased` section unless the user has specified the release version.
- Write clear, user-facing notes describing additions, changes, fixes, and relevant compatibility limitations.
- Keep implementation status and hardware validation status accurate.
- Determine build versions using the build version policy below. Do not invent a release date, successful test, or published release.
- Present the feature's release-ready changelog directly in the handoff to the user; do not provide only a file path.
- Include that same changelog in the proposed PR description and, after authorization, in the actual PR body.

Preparing a changelog does not authorize creating a tag or publishing a release.

## Tests and evidence

Run the checks and tests appropriate to the change and required by the relevant workflow specification.

- Use `PASS`, `FAIL`, `NOT RUN`, or `BLOCKED` when recording validation.
- Treat a test as successful only when it actually passed.
- Identify hardware results reported by the user as user-reported.
- Preserve pending tests until evidence resolves them.
- Do not invent console models, firmware versions, Infinity versions, commands, hashes, or evidence.
- A successful build does not establish physical installation, WPA2 connectivity, or Infinity startup.
- Keep hardware-dependent acceptance tests pending when no device or valid evidence is available.
- Do not claim universal support based on a single tested console or router.

If a required test fails, fix the issue within scope and rerun the relevant checks. If a required test is blocked or cannot be run, report that limitation and do not treat it as a successful test.

## Generated artifacts and Git hygiene

- Keep source code, build/test scripts, release notes, licenses and required provenance inputs in the source repository. Generated deliverables and full execution logs belong outside source commits.
- Use `builds/` for final packages, corresponding source ZIPs, matching PSP executables, release manifests and SHA-256 files. Use `artifacts/` for intermediate trees, full logs, machine-generated verification results and raw hardware diagnostic reports. Neither directory is a source input that should be committed.
- Do not retain duplicate final ZIPs, release manifests or hash lists in `artifacts/` after migrating them to `builds/`. Verify identical hashes, remove the redundant copies, update active references and ensure the packager writes final deliverables only to `builds/`. Intermediate build outputs and the metadata required to verify them remain in `artifacts/`.
- Store concise implementation/validation summaries, build identities and pending tests in `context/`, following its existing ignore policy. Keep the root README general. Do not force-add ignored records without explicit authorization.
- Distribute the binary ZIP, corresponding source ZIP, manifest and hashes as assets of an explicitly authorized GitHub release. Full CI logs and temporary outputs can be retained as CI artifacts with appropriate retention; they do not need to be checked into Git. A local cleanup does not authorize publishing anything.
- Preserve inherited `.a`, `.prx`, `.bin`, headers and resources that are required build inputs and documented by the upstream inventory. Do not remove or globally ignore all binary files, ZIPs, JSON, text files or manifests merely by extension; classify their purpose first.
- Before removing generated files from version control, inspect `git ls-files` and `git check-ignore`. If an output is already tracked, an ignore rule alone does not untrack it. Within an authorized cleanup, remove only the confirmed generated paths from the index and retain local packages/evidence needed by ongoing work.
- Delete only disposable caches or confirmed redundant output. Preserve the latest delivered packages, their hashes, failed-build evidence useful for investigation, and required build metadata. Verify matching hashes before removing a duplicate package.
- Keep cleanup targets inside the resolved project root and avoid symlink/reparse-point traversal. Preserve previous ignore rules and use narrowly scoped rules for new output locations. Use `builds/` as the canonical final output directory; `/build/` is ignored only as a legacy guard.
- Do not rewrite Git history or delete release assets as part of routine cleanup. Already committed artifacts remain in historical commits unless a separate history-rewrite task is explicitly authorized.
- Verify that intended output paths are ignored, required build inputs remain available to Git, package checks still pass and `git diff --check` is clean. Documentation/ignore/cache-only cleanup does not require a firmware rebuild or a new product version.

## Required compiled build before commit and PR

For every firmware feature or fix, produce the compiled files needed for the user to test the change on their PSP before requesting authorization to commit or open a PR.

### Build location and contents

Place the final deliverables in the repository-root `builds/` directory. At minimum, provide the matching files from the same build:

- `builds/PSP/GAME/PROUPDATE/EBOOT.PBP`
- `builds/PSP/GAME/FastRecovery/EBOOT.PBP`

Also leave an installable ZIP in `builds/`, using the actual product version and target firmware in its filename, together with SHA-256 hashes and a small build manifest identifying the source/build state and toolchain.

Existing scripts may use other locations for intermediate output, but the final files handed to the user must exist in `builds/`. Do not mix old and new EBOOT files, rename an older binary as a new version, or provide an empty directory as the deliverable.

Keep generated binaries and packages out of source commits. Preserve existing ignore rules and add a narrowly scoped `builds/` rule if needed to prevent accidental inclusion. This does not authorize force-adding ignored workflow or context records.

### Build version policy

Version numbering applies to generated product builds. It must not increment because a commit, push, PR, documentation update, or release action occurs.

The migration starting point is `v0.1.0-beta.1`. The first build for the next feature ticket must be **`v0.2.0`**, without `-beta.1` or another prerelease suffix.

For each subsequent distinct feature ticket delivered as a build:

- Increment the second numeric component (MINOR) by one.
- Keep the first component unchanged and reset the third component to zero.
- Omit beta, alpha, and release-candidate suffixes unless the user explicitly changes this policy.
- Apply the same rule to tickets classified as fixes; do not choose a PATCH increment instead.

Examples:

| Ticket/build milestone | Product build version |
| --- | --- |
| Existing version when this policy was adopted | `v0.1.0-beta.1` |
| First new feature ticket | `v0.2.0` |
| Next distinct feature ticket | `v0.3.0` |
| Following distinct feature ticket | `v0.4.0` |

Before assigning a version, inspect the latest delivered build metadata and relevant `context/` records. Do not rely only on the latest Git tag: a tested build may not have been tagged or published. Once a later build exists, continue from it rather than resetting to `v0.2.0`.

Assign one product version per feature ticket when generating its first build. Rebuilds, corrections, and test iterations of that same ticket retain its assigned version. Distinguish them with a separate build identifier and hashes in the manifest and context record. This identifier must not reintroduce a beta/alpha suffix into the product version.

Do not consume another feature version for a failed compilation or for merely rerunning packaging. A new ticket with a new delivered build receives the next MINOR version.

Keep the product version consistent across build/package metadata, package filenames, manifests, applicable installer/Fast Recovery product labels, and the version reported to the user. Use conventional package filenames such as `pro-x-0.2.0-661.zip`; retain the `v` prefix in the human-facing version identifier.

This product numbering must not alter Sony firmware version 6.61, PRO HEN compatibility values, configuration magic, ABI, or the contracts required by Infinity. Do not lengthen a patched display string without verifying its destination capacity. Record the new product version in supported build metadata rather than changing compatibility fields.

Record each delivered build's version and identity in the implementation record. Reference the actual build version in the feature handoff and changelog while keeping unpublished work clearly identified as unreleased. Removing a prerelease suffix does not establish hardware validation or a published stable release.

Version assignment and building do not authorize commits, pushes, tags, PRs, or releases. Keep the existing hardware-test and explicit-authorization gates unchanged.

### Preliminary handoff for the user's PSP test

After compilation and the required local checks pass:

1. Verify the compiled files and package actually exist and match the current implementation and the ticket's product version assigned before compilation.
2. Leave them in `builds/` and give the user their exact paths or accessible download links.
3. Explain briefly what changed, which files to copy, how to install/update using the actual installer behavior, and how to test the feature.
4. State which checks passed locally and which hardware tests are still pending.
5. Record this build identity and pending validation in `context/`.
6. Wait for the user to test it and report the outcome.

Do not postpone delivering the build until after commit, push, or PR creation. This preliminary delivery is explicitly allowed while hardware validation is pending; it does not mean all acceptance tests passed.

If a required local check fails, fix it and rerun the affected checks before presenting a package as ready for the user's normal feature test. Clearly distinguish any separately requested diagnostic build from a validated test candidate.

If the user reports a failure, investigate, fix it within scope, rebuild, refresh the files in `builds/`, and repeat the relevant local and hardware checks. Retain enough build identity in the context record to distinguish test attempts.

A statement that the feature works is a test result, not permission to commit or publish. Once the required tests, including the user's PSP validation for the delivered firmware change, have passed, prepare the final changelog and PR proposal and ask for explicit authorization as described below.

If firmware-affecting changes occur after the tested build, regenerate the deliverables and repeat the affected validation before requesting approval for those changes.

## Mandatory authorization before commit and PR

Implementation and local validation are separate from committing and publishing changes.

Do not create a commit, push the feature branch, or open a PR until BOTH conditions are met:

1. All required tests and checks for the requested change have passed, including the user's PSP test of the compiled firmware delivered in `builds/` when the change affects firmware.
2. The user has given explicit authorization for the reviewed change set after being shown the result.

Before requesting authorization:

- Finish the authorized implementation and required documentation updates.
- Deliver the compiled firmware in `builds/` first and record the user's test results for firmware changes.
- Review the diff and the test results.
- Present a concise change summary, test results, limitations, the release changelog, and the proposed commit/PR title and PR description.
- Make the proposed PR description concrete and include its changelog.
- Identify the intended branch and PR target when available.

Then explicitly ask whether the user authorizes creating the commit, pushing the branch, and opening the PR. Wait for an affirmative answer.

An instruction to implement a feature, silence, elapsed time, or approval of an earlier feature is not authorization to commit or publish this feature.

If required tests fail or remain blocked, report the status and continue the permitted work; do not open a PR, including a draft PR, and do not treat approval as a substitute for passing the required tests.

After explicit approval, create the commit, push the branch, and open the PR with the reviewed changelog included. Do not request the same approval again for the unchanged reviewed change set.

If material changes are needed after approval, rerun the affected checks and obtain renewed authorization for the revised change set before committing or publishing it.

Do not merge the PR, create a release tag, or publish a release unless separately instructed.

## Final handoff and continuity

Before the authorization question, provide:

- What changed and why.
- Tests performed and their results.
- The implementation record and confirmation that `context/README.md` was updated.
- Whether the root README was updated or reviewed without changes.
- The release-ready changelog.
- Remaining limitations and exact paths to the compiled deliverables in `builds/`, including their build identity and hashes.
- The proposed commit and PR information.

After authorized publication, provide the commit identifier, PR link, and any remaining limitations. Keep implementation records accurate about what was actually committed, pushed, or published.

Respect existing Git ignore rules. Maintaining local `workflow/` or `context/` files does not require force-adding them or changing ignore rules.

Do not conclude feature work with only a chat response: leave the required context records current so another chat can resume the project.
