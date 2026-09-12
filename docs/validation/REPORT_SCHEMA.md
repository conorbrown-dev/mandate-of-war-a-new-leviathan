# Validation Report Schema v1

Every run writes `validation/artifacts/<scenarioId>/<runId>/report.json` and returns zero only when the report status is `PASS`.

Required fields are `schemaVersion`, `scenarioId`, `runId`, `status`, `seed`, `startedAt`, `durationMs`, non-empty `assertions`, `metrics`, `artifacts`, and `errors`. Gameplay reports also record `description`, `checkpoints`, `environment`, and `warnings`.

Assertions contain a stable `id`, `PASS` or `FAIL` status, a diagnostic message, and optional observed values. Artifact paths are relative to the run directory. The wrapper rejects missing fields, mismatched scenario/run IDs, empty assertions, Godot `SCRIPT ERROR` output, and report/process disagreement.
