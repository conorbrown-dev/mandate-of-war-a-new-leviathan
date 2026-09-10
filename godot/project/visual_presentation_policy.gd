class_name VisualPresentationPolicy
extends RefCounted

## Scale profiles measure the supported batched render path. Prototype wrappers
## are intentionally excluded: _spawn_units() has no per-unit visual metadata,
## and wrapper nodes are not a scalable profile representation.
static func use_prototype_wrappers(requested: bool, is_scale_profile: bool) -> bool:
	return requested and not is_scale_profile
