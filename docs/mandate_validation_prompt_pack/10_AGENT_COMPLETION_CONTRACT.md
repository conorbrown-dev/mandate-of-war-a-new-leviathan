# Prompt 10 — Agent Completion Contract and Repository Instructions

## Goal

Update the repository's agent instructions so Codex, MIRIAM, and future coding agents cannot casually declare gameplay work complete without validation.

Inspect the existing `AGENTS.md` and related project instructions first. Preserve useful existing guidance.

Add a concise section equivalent to the following, adapted to repository terminology.

---

## Required Gameplay Validation Contract

A gameplay feature is not complete merely because:

- the project builds;
- the editor opens;
- static analysis passes;
- unit tests unrelated to the feature pass;
- the code appears correct by inspection.

When a relevant automated validation scenario exists, the agent MUST run it before claiming completion.

When implementing or materially changing gameplay behavior, the agent SHOULD add or update a validation scenario unless the behavior is already adequately covered.

Validation should prefer this evidence hierarchy:

```text
state assertions
+ deterministic/reproducible scenario execution
+ screenshots for visual checkpoints
+ video for motion/sequence when useful
+ performance metrics for scale-sensitive systems
```

Screenshots and videos are evidence; they do not replace state assertions.

### Failure handling

An agent MUST NOT make a failed validation pass by silently:

- removing assertions;
- skipping the test;
- increasing tolerances without justification;
- regenerating visual baselines;
- changing expected output to match a bug;
- bypassing the real production path with test-only behavior.

If the agent believes the acceptance criteria are incorrect, it must report the conflict and explain the proposed correction.

### Required completion summary

For gameplay work, the final response must include:

```text
Implementation:
Validation scenarios run:
PASS/FAIL:
State assertions:
Visual artifacts:
Performance evidence:
Known limitations:
Files changed:
```

If a required scenario fails, the agent must state FAIL even if the code compiles.

### Token/time discipline

Do not repeatedly guess at gameplay behavior.

When validation fails:

1. read the failure report;
2. inspect the smallest relevant logs/state;
3. form one concrete hypothesis;
4. make the smallest justified change;
5. rerun the narrow scenario;
6. only broaden investigation if it still fails.

Prefer:

```text
tools/validate <one-scenario>
```

over repeatedly running the entire project/test suite during iteration.

Do not regenerate videos on every iteration unless the failure is visual/behavioral and video materially helps.

Do not run expensive scale benchmarks after every small edit. Run the narrow functional scenario first.

---

## Also create a reusable agent command reference

Create:

```text
docs/validation/AGENT_WORKFLOW.md
```

It should explain the shortest workflow for:

```text
implement feature
-> run narrow validation
-> inspect evidence
-> fix
-> rerun
-> run broader regression
-> report result
```

Include real repository commands, not placeholders.

## Completion gate

Run one existing validation scenario after modifying the agent documentation to ensure the documented commands are still correct.

Final response:

```text
AGENT CONTRACT RESULT: PASS | FAIL

Agent instruction files changed:
Workflow documentation:
Validation command checked:
Scenario result:
```
