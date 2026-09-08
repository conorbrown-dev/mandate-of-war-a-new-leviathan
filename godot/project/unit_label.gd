extends Label3D

var world_offset: Vector3 = Vector3(0.0, 1.0, 0.0)

func _process(_delta: float) -> void:
	global_position = get_parent().global_position + world_offset
