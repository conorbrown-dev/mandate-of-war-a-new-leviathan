extends Node3D

@export var unit_id: int = -1
@export var unit_color := Color(0.18, 0.68, 1.0)

@onready var mesh_instance: MeshInstance3D = $MeshInstance3D


func set_selected(selected: bool) -> void:
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(1.0, 0.78, 0.12) if selected else unit_color
	material.roughness = 0.62
	mesh_instance.material_override = material
