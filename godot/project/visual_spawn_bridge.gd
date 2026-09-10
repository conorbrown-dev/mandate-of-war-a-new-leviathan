class_name VisualSpawnBridge
extends RefCounted

const VisualRootScript = preload("res://unit_visual_root.gd")

## The sole presentation entry point for normal scenario/factory/debug spawns.
## It accepts stable unit/visual identity supplied by gameplay data; it never
## reads a GLB path from a scenario, replay, or network message.
func spawn(parent: Node, registry, visual_id: String, world_transform: Transform3D, faction_color: Color) -> Node3D:
	var wrapper = VisualRootScript.new()
	parent.add_child(wrapper)
	wrapper.configure(registry, visual_id, faction_color)
	wrapper.apply_simulation_transform(world_transform)
	return wrapper
