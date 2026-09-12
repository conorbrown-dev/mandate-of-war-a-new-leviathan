# Prompt 11 — Codex Independent Review Gate

> Recommended usage: run this prompt with Codex after MIRIAM completes one or more validation-framework goals.

You are acting as an **independent reviewer**, not the original implementer.

Do not assume the previous agent's completion claims are correct.

## Goal

Determine whether the Mandate gameplay-validation system actually prevents false completion claims and whether it exercises the real production game.

## Review steps

### 1. Read the contract

Read:

```text
docs/validation/VALIDATION_BASELINE.md
docs/validation/RUNNING_TESTS.md
docs/validation/SCENARIO_CATALOG.md
docs/validation/AGENT_WORKFLOW.md
AGENTS.md
```

Read only files that actually exist; report missing required documentation.

### 2. Inspect implementation boundaries

Confirm scenarios exercise production systems rather than test-only duplicates.

Look specifically for:

- fake units used instead of production units;
- direct state mutation that bypasses the command being tested;
- validation-specific branches in production gameplay;
- assertions that merely confirm setup;
- swallowed exceptions;
- tests that always return success;
- reports whose status can disagree with process exit status;
- screenshot files written without validating capture success;
- visual baseline auto-update behavior;
- unbounded waits;
- arbitrary real-time sleeps;
- non-seeded randomness;
- fragile absolute filesystem paths.

### 3. Run the narrow test suite

Run the documented validation commands.

Record exact results.

### 4. Prove failure detection

For one scenario, introduce a **temporary local mutation** to an expected assertion or test value.

Do not commit this mutation.

Verify:

```text
scenario fails
process exit is non-zero
report status is FAIL
failure reason is understandable
```

Then revert the mutation and verify PASS.

If you cannot safely perform this test, explain why.

### 5. Inspect artifacts

Open at least one produced screenshot.

If video support exists, confirm the expected file is produced and is non-empty. Do not spend tokens narrating every frame.

Confirm artifact paths in `report.json` resolve correctly.

### 6. Review determinism

Run one scenario twice with the same seed.

Identify any meaningful differences.

Do not demand byte-identical screenshots unless the system explicitly promises them.

### 7. Review scope

The framework should be small enough that gameplay developers/agents can use it routinely.

Flag excessive abstraction, unnecessary plugins, duplicated engines/frameworks, or complicated workflows that will cause agents to avoid using it.

## Review output

Create:

```text
docs/validation/CODEX_VALIDATION_REVIEW.md
```

Use severity:

```text
BLOCKER
HIGH
MEDIUM
LOW
```

Every finding must contain:

```text
evidence/path
why it matters
smallest recommended correction
```

## Decision

End with exactly one:

```text
REVIEW DECISION: ACCEPT
```

or:

```text
REVIEW DECISION: REJECT
```

REJECT if any of these are true:

- required validation can falsely PASS;
- scenario exit status does not propagate failure;
- primary scenarios do not exercise production behavior;
- screenshots/reports claim artifacts that do not exist;
- tests rely on uncontrolled indefinite waits;
- agents can automatically overwrite failed visual baselines;
- basic validation cannot be run from the command line.

Do not fix major issues during the review unless explicitly asked. The point of this prompt is an independent evaluation.
