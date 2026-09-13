# Goal 05 Modding, Asset, and Map Architecture Review

**Date:** 2026-09-12

| Area | Finding | Disposition |
|---|---|---|
| Mod safety | Manifest scripts are never executed; generated-unit paths reject traversal and require a matching mesh. | Accepted for current data-only scope. |
| Versioning | Full SHA-256 content handles are idempotent; dependency batches validate versions, order deterministically, and stage atomically. | Accepted. |
| Asset reproducibility | Generator output records a canonical-definition hash and generator version. | Accepted for placeholder pipeline. |
| Map longevity | Native YAML/binary map bundles round-trip terrain, resources, spawns, and entities; editor export is reloaded by native MapLoader. | Accepted. |
| Editor boundary | Standalone editor state and scene do not alter skirmish startup; water persists in drafts. | Accepted; native format has no water sidecar yet, so water remains editor-draft metadata. |

## Review conclusion

No Goal 05 blocker remains for the documented placeholder/data-only scope.
Future scripting requires a separately reviewed deterministic sandbox, and
native water serialization is future map-format expansion rather than an
implicit compatibility claim.
