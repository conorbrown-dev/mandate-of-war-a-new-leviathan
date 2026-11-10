extends Node3D
## Presents visibility-filtered native events. No damage, targeting or RNG here.

const Palette := preload("res://audio/audio_palette.gd")
const SmokeShader := preload("res://combat_smoke.gdshader")
const MAX_EFFECTS := 96
const MAX_VOICES := 12
const MAX_SOUND_DISTANCE := 4500.0
const LIFETIMES := [0.14, 0.4, 2.4]
const SOUND_PATHS := [
	"res://audio/kenney_scifi/lowFrequency_explosion_000.ogg",
	"res://audio/kenney_impact/impactMetal_medium_000.ogg",
	"res://audio/kenney_scifi/explosionCrunch_000.ogg",
]

var viewer := 0
var height_at: Callable
var position_visible: Callable
var listener: Camera3D
var cursor := 0
var _active: Array[Dictionary] = []
var _voices: Array[AudioStreamPlayer3D] = []
var _voice_events: Array[Dictionary] = []
var _streams: Array[AudioStream] = []
var _shot_streams: Dictionary = {}
var _next_voice := 0
var _last_sound_ms := [-1000, -1000, -1000]
var _sphere: SphereMesh

func _ready() -> void:
	_sphere = SphereMesh.new()
	_sphere.radius = 1.0
	_sphere.height = 2.0
	_sphere.radial_segments = 12
	_sphere.rings = 6
	for path in SOUND_PATHS:
		_streams.append(load(path))
	for path: String in Palette.SHOTS.values():
		if not _shot_streams.has(path):
			_shot_streams[path] = load(path)
	for index in range(MAX_VOICES):
		var voice := AudioStreamPlayer3D.new()
		voice.name = "CombatVoice%d" % index
		voice.playback_type = AudioServer.PLAYBACK_TYPE_STREAM
		voice.volume_db = -16.0
		voice.bus = "Combat"
		voice.unit_size = 300.0
		voice.max_distance = MAX_SOUND_DISTANCE
		add_child(voice)
		_voices.append(voice)
		_voice_events.append({})

func _can_present(event: Dictionary) -> bool:
	return int(event.faction) == viewer or (position_visible.is_valid() and bool(position_visible.call(event)))

func consume(packet: Dictionary) -> void:
	var now_tick := int(packet.get("tick", 0))
	for event: Dictionary in packet.get("events", []):
		var sequence := int(event.sequence)
		if sequence <= cursor:
			continue
		cursor = sequence
		var kind := int(event.kind)
		if kind < 0 or kind >= LIFETIMES.size() or not _can_present(event):
			continue
		var age := maxf(0.0, float(now_tick - int(event.tick)) * 0.05)
		if age >= LIFETIMES[kind]:
			continue
		var ground := float(height_at.call(float(event.x), float(event.y))) if height_at.is_valid() else 0.0
		var altitude := 160.0 if bool(event.get("airborne", false)) else 2.0
		var origin := Vector3(float(event.x), ground + altitude, float(event.y))
		if _active.size() < MAX_EFFECTS:
			_spawn(event, origin, age)
		# Delayed polling must not replay a backlog of sounds.
		if age <= 0.2:
			_play_sound(event, origin)
	cursor = maxi(cursor, int(packet.get("cursor", cursor)))

func _material(color: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.albedo_color = color
	return material

func _mesh(parent: Node3D, color: Color) -> MeshInstance3D:
	var mesh := MeshInstance3D.new()
	mesh.mesh = _sphere
	mesh.material_override = _material(color)
	mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	parent.add_child(mesh)
	return mesh

func _spawn(event: Dictionary, origin: Vector3, age: float) -> void:
	var kind := int(event.kind)
	var effect := Node3D.new()
	effect.name = ["MuzzleFlash", "Impact", "Destruction"][kind]
	add_child(effect)
	effect.global_position = origin
	var flash := _mesh(effect, Color(1.0, 0.36, 0.06))
	var smoke: MeshInstance3D
	var tracer: MeshInstance3D
	var direction := Vector3(float(event.vx), 0, float(event.vy)).normalized()
	if kind == 0:
		flash.position = direction * 5.0
		var line := ImmediateMesh.new()
		line.surface_begin(Mesh.PRIMITIVE_LINES, _material(Color(1.0, 0.83, 0.4)))
		line.surface_add_vertex(Vector3.ZERO)
		line.surface_add_vertex(direction * 12.0)
		line.surface_end()
		tracer = MeshInstance3D.new()
		tracer.mesh = line
		tracer.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		effect.add_child(tracer)
	elif kind == 2:
		smoke = _mesh(effect, Color(0.38, 0.4, 0.42, 0.85))
		var smoke_material := ShaderMaterial.new()
		smoke_material.shader = SmokeShader
		smoke.material_override = smoke_material
	_active.append({"node": effect, "flash": flash, "smoke": smoke, "tracer": tracer,
		"event": event, "age": age, "direction": direction})
	_update_effect(_active.back())

func _update_effect(effect: Dictionary) -> void:
	var kind := int(effect.event.kind)
	var age := float(effect.age)
	var fraction := age / float(LIFETIMES[kind])
	var radius := [1.8, 2.5, 10.0][kind] as float
	var flash: MeshInstance3D = effect.flash
	flash.scale = Vector3.ONE * maxf(0.01, radius * (1.0 + fraction))
	var material := flash.material_override as StandardMaterial3D
	var color := material.albedo_color
	color.a = maxf(0.0, 1.0 - age / (0.3 if kind == 2 else float(LIFETIMES[kind])))
	material.albedo_color = color
	flash.visible = color.a > 0.001
	if effect.smoke != null:
		var smoke: MeshInstance3D = effect.smoke
		smoke.visible = age > 0.1
		smoke.position.y = age * 7.0
		smoke.scale = Vector3(1, 1.3, 1) * (5.0 + age * 6.0)
		var smoke_material := smoke.material_override as ShaderMaterial
		smoke_material.set_shader_parameter("opacity", 0.85 * (1.0 - fraction))
		smoke_material.set_shader_parameter("age", age)
	if effect.tracer != null:
		effect.tracer.position = effect.direction * (5.0 + age * Vector2(float(effect.event.vx), float(effect.event.vy)).length())

func _play_sound(event: Dictionary, origin: Vector3) -> void:
	if listener == null or listener.global_position.distance_to(origin) > MAX_SOUND_DISTANCE:
		return
	var kind := int(event.kind)
	var now := Time.get_ticks_msec()
	if now - int(_last_sound_ms[kind]) < 60:
		return
	_last_sound_ms[kind] = now
	var voice := _voices[_next_voice]
	_voice_events[_next_voice] = event
	_next_voice = (_next_voice + 1) % MAX_VOICES
	voice.global_position = origin
	voice.stop()
	voice.stream = _streams[kind]
	if kind == 0:
		var path := Palette.shot_path(int(event.get("unit_type", -1)))
		if not _shot_streams.has(path):
			return
		voice.stream = _shot_streams[path]
	voice.play()

func _process(delta: float) -> void:
	for index in range(_active.size() - 1, -1, -1):
		var effect := _active[index]
		effect.age = float(effect.age) + delta
		if float(effect.age) >= float(LIFETIMES[int(effect.event.kind)]) or not _can_present(effect.event):
			effect.node.queue_free()
			_active.remove_at(index)
		else:
			_update_effect(effect)
	for index in range(_voices.size()):
		if _voices[index].playing and not _voice_events[index].is_empty() and not _can_present(_voice_events[index]):
			_voices[index].stop()

func clear() -> void:
	for effect in _active:
		effect.node.free()
	_active.clear()
	for index in range(_voices.size()):
		_voices[index].stop()
		_voices[index].stream = null
		_voice_events[index] = {}
	_next_voice = 0
	cursor = 0
	_last_sound_ms = [-1000, -1000, -1000]

func _exit_tree() -> void:
	clear()
	_streams.clear()
	_shot_streams.clear()
