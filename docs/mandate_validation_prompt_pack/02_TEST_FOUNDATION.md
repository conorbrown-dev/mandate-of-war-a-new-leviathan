# Prompt 02 — Build the Test Foundation

Read `docs/validation/VALIDATION_BASELINE.md` first.

## Goal

Establish the minimum reliable automated test foundation needed for scene-level gameplay validation.

Prefer GdUnit4 if the baseline confirms it is appropriate for this repository. If another existing framework already provides equivalent capabilities, justify reusing it instead.

## Required capabilities

The repository must have a documented command that can:

- run all validation-related tests;
- run one test suite or validation area;
- return a non-zero exit status on failure;
- produce readable console output;
- work without manually opening the Godot editor.

Where appropriate, provide thin repository-owned wrappers such as:

```text
tools/test
tools/test.ps1
tools/test.sh
```

or an equivalent cross-platform solution consistent with the repository.

Mandate targets Windows, macOS, and Linux, so do not make the validation workflow permanently Windows-only or Linux-only.

## Scene-level test proof

Create one small scene-level smoke test using a **real existing Mandate scene or gameplay subsystem** identified in the baseline.

The smoke test must demonstrate at least:

```text
load scene
-> obtain a real gameplay node/service
-> perform or trigger one meaningful operation
-> assert observable state
-> exit cleanly
```

If input simulation is already practical, include one real input/action step. Do not add fake gameplay code solely for the test.

## Test isolation

Ensure validation tests do not unintentionally depend on:

- previous test order;
- prior user save data;
- editor state;
- arbitrary wall-clock sleeps;
- a specific developer machine path.

Where global/singleton state exists, document how it is reset.

## Documentation

Create or update:

```text
docs/validation/RUNNING_TESTS.md
```

Include the exact commands for:

- all tests;
- one validation suite;
- verbose/debug output if available;
- headless/non-rendering tests where supported.

## Do not overbuild

Do not implement screenshots, video, visual diffs, or the full scenario abstraction in this step.

## Completion gate

Run the smoke test from the command line.

A successful build alone is **not** completion.

Final response format:

```text
TEST FOUNDATION RESULT: PASS | FAIL

Evidence:
- test command:
- suites run:
- tests passed:
- tests failed:
- relevant log/report paths:

Files changed:
Known limitations:
Next step:
```

If the smoke test fails, report FAIL. Do not describe the task as complete.
