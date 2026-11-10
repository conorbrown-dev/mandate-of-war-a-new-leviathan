extends Node3D
## Nearest audible, currently visible moving units only. No hidden-state reads.
const Palette := preload("res://audio/audio_palette.gd")
const MAX_VOICES := 8
const MAX_DISTANCE := 1800.0
const SAMPLE_SECONDS := 0.1
var _voices: Array[AudioStreamPlayer3D] = []
var _owners: Array[int] = []
var _streams: Dictionary = {}
var _previous: Dictionary = {}
var _last_tick := -1
var _elapsed := 0.0

func _ready() -> void:
	for path in [Palette.GROUND_ENGINE, Palette.AIR_ENGINE]:
		var stream := load(path).duplicate() as AudioStreamOggVorbis
		stream.loop = true
		_streams[path] = stream
	for index in range(MAX_VOICES):
		var voice := AudioStreamPlayer3D.new()
		voice.name = "EngineVoice%d" % index
		voice.bus = "Engines"
		voice.playback_type = AudioServer.PLAYBACK_TYPE_STREAM
		voice.unit_size = 80.0
		voice.max_distance = MAX_DISTANCE
		voice.volume_db = -26.0
		add_child(voice)
		_voices.append(voice)
		_owners.append(-1)

func update_from_view(view: Node, delta: float) -> void:
	if not view.match_started or view.match_finished:
		clear()
		return
	# Sight loss/removal is immediate, even between the 10 Hz allocation passes.
	for index in range(MAX_VOICES):
		if _owners[index] >= 0 and (not view.entity_to_instance.has(_owners[index]) or not view._is_unit_shown(_owners[index])):
			_release(index)
	_elapsed += delta
	if _elapsed < SAMPLE_SECONDS:
		return
	_elapsed = 0.0
	var clock: PackedFloat64Array = view.extension.call("get_tick_clock")
	var tick := int(clock[0])
	if tick == _last_tick:
		return
	var sim_seconds := float(tick - _last_tick) * 0.05
	_last_tick = tick
	var airborne: PackedByteArray = view.extension.call("get_airborne_flags", view.entity_ids)
	var candidates: Array[Dictionary] = []
	var current := {}
	for index in range(view.entity_ids.size()):
		var id := int(view.entity_ids[index])
		if not view._is_unit_shown(id):
			continue
		var unit_type := int(view.entity_unit_types.get(id, -1))
		var path := Palette.engine_path(unit_type)
		if path.is_empty():
			continue
		var position: Vector3 = view._unit_overlay_anchor(id) - Vector3.UP * 5.0
		current[id] = position
		var speed := 0.0
		if _previous.has(id) and sim_seconds > 0.0:
			speed = position.distance_to(_previous[id]) / sim_seconds
		# A newly discovered unit doesn't generate a teleport/rev sound.
		var flying := index < airborne.size() and airborne[index] != 0
		if not flying and speed < 0.5:
			continue
		var distance: float = view.camera.global_position.distance_squared_to(position)
		if distance > MAX_DISTANCE * MAX_DISTANCE:
			continue
		candidates.append({"id": id, "position": position, "distance": distance, "path": path,
			"pitch": 0.85 + 0.3 * clampf(speed / Palette.cruising_speed(unit_type), 0.0, 1.0)})
	_previous = current
	apply_candidates(candidates)

func apply_candidates(candidates: Array[Dictionary]) -> void:
	candidates.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		return a.distance < b.distance if a.distance != b.distance else a.id < b.id)
	var wanted := {}
	for index in range(mini(MAX_VOICES, candidates.size())):
		wanted[int(candidates[index].id)] = candidates[index]
	for index in range(MAX_VOICES):
		if not wanted.has(_owners[index]):
			_release(index)
	for id: int in wanted:
		var index := _owners.find(id)
		if index < 0:
			index = _owners.find(-1)
		var candidate: Dictionary = wanted[id]
		var voice := _voices[index]
		voice.global_position = candidate.position
		voice.pitch_scale = float(candidate.pitch)
		if _owners[index] != id:
			_owners[index] = id
			voice.stream = _streams[candidate.path]
			voice.volume_db = -50.0
			voice.play()
		voice.set_meta("target_db", -20.0 if candidate.path == Palette.AIR_ENGINE else -26.0)

func _process(delta: float) -> void:
	for voice in _voices:
		if voice.playing:
			voice.volume_db = move_toward(voice.volume_db, float(voice.get_meta("target_db", -26.0)), delta * 60.0)

func _release(index: int) -> void:
	_voices[index].stop()
	_voices[index].stream = null
	_owners[index] = -1

func clear() -> void:
	for index in range(_voices.size()):
		_release(index)
	_previous.clear()
	_last_tick = -1
	_elapsed = 0.0

func _exit_tree() -> void:
	clear()
	_streams.clear()
