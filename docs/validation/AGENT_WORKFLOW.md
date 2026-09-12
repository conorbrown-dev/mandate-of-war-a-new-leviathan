# Agent Gameplay Validation Workflow

Use the smallest scenario that proves the changed behavior.

```text
implement the production change
-> run one narrow scenario
-> read report.json assertions/errors
-> form one concrete hypothesis
-> make the smallest justified fix
-> rerun the same scenario
-> run tools/test for broader regression
-> report evidence and limitations
```

Commands:

```bash
tools/validate --list
tools/validate <scenario-id>
tools/validate all
tools/test native
tools/test godot
tools/test
```

For visual work, first pass state assertions, then use a rendered run:

```bash
tools/validate strategic_zoom_transition --rendered --require-screenshots
```

Use `--record` only when motion/sequence evidence is useful. Use `--update-baseline` only when the assigned feature explicitly changes an accepted visual and the new image has been inspected. Never update a baseline merely to clear a comparison failure.

In the completion report, identify the scenario IDs, PASS/FAIL, state checks, artifact paths, relevant benchmark results, and anything not proven in the current environment. A build-only result is not gameplay completion.
