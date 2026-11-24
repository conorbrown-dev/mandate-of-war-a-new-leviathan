extends Node
## The command network's voice. It turns native radio reports (get_radio_events)
## into one spoken line at a time with matching text. Local presentation only:
## what it says, queues or drops never reaches the simulation, a replay or the
## network. A line with no recorded clip is still shown as text.

const Location := preload("res://audio/radio/radio_location.gd")
const Prefs := preload("res://audio/radio/radio_prefs.gd")
const CATALOG_PATH := "res://audio/radio/radio_catalog.json"
const VOICE_MANIFEST_PATH := "res://audio/radio/voice_manifest.json"
const MAX_QUEUE := 6
const MAX_HISTORY := 40
const TICK_SECONDS := 0.05
## Pause between two lines, so they never run together.
const LINE_GAP_SECONDS := 0.35
const TEXT_SECONDS_PER_WORD := 0.38
const TEXT_MIN_SECONDS := 2.5
const VOICE_DB := -6.0
const ATTENTION_DB := -20.0

## A line began (voiced or text only). The dictionary is the history entry.
signal announcement_started(entry: Dictionary)
## The line finished, was preempted or was cleared.
signal announcement_finished(entry: Dictionary)
signal history_changed

var prefs := Prefs.new()
var listener_faction := 0
## The battlefield in simulation metres (x, y), for grid references.
var map_rect := Rect2()
var cursor := 0
var current: Dictionary = {}
var history: Array[Dictionary] = []
## Counts for tests and the performance note.
var stats := {"received": 0, "spoken": 0, "text_only": 0, "deduped": 0, "stale": 0, "overflow": 0, "expired": 0, "preempted": 0, "filtered": 0, "unknown": 0}

## Recorded lines: clip id -> resource path, from the generator's manifest.
## A line whose id is missing here, or whose file is gone, is shown as text.
var clips: Dictionary = {}

var _catalog: Dictionary = {}
var _events_by_kind: Dictionary = {}
var _events_by_id: Dictionary = {}
var _queue: Array[Dictionary] = []
var _last_accepted: Dictionary = {}
var _variant_turn: Dictionary = {}
var _clock := 0.0
var _remaining := 0.0
var _gap := 0.0
var _voice: AudioStreamPlayer
var _attention: AudioStreamPlayer


func _ready() -> void:
	_voice = AudioStreamPlayer.new()
	_voice.name = "RadioVoice"
	_voice.bus = "Voice"
	_voice.volume_db = VOICE_DB
	add_child(_voice)
	_attention = AudioStreamPlayer.new()
	_attention.name = "RadioAttention"
	_attention.bus = "Voice"
	_attention.volume_db = ATTENTION_DB
	add_child(_attention)
	if _catalog.is_empty():
		load_catalog(CATALOG_PATH)
		load_voice_manifest(VOICE_MANIFEST_PATH)


## Returns "" on success or why the catalog was refused. A refused catalog
## leaves the previous one in place.
func load_catalog(path: String) -> String:
	if not FileAccess.file_exists(path):
		return "missing catalog %s" % path
	var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(path))
	if not (parsed is Dictionary):
		return "catalog is not a JSON object"
	return use_catalog(parsed)


func use_catalog(catalog: Dictionary) -> String:
	if int(catalog.get("schema_version", 0)) != 1:
		return "unsupported schema_version"
	var by_kind := {}
	var by_id := {}
	for event: Variant in catalog.get("events", []):
		if not (event is Dictionary) or String(event.get("id", "")).is_empty():
			return "an event has no id"
		var id := String(event.id)
		if by_id.has(id):
			return "duplicate event id %s" % id
		if not catalog.get("severities", {}).has(event.get("severity", "")):
			return "%s has an unknown severity" % id
		by_id[id] = event
		if String(event.get("status", "active")) == "active" and event.has("native_kind"):
			by_kind[int(event.native_kind)] = event
	_catalog = catalog
	_events_by_kind = by_kind
	_events_by_id = by_id
	if _attention != null:
		var cue := String(catalog.get("attention_cue", ""))
		_attention.stream = load(cue) if not cue.is_empty() and ResourceLoader.exists(cue) else null
	return ""


## A voice pack's manifest (see tools/generate_radio_voice.py). Replaces the
## known clips; returns how many were listed.
func load_voice_manifest(path: String) -> int:
	clips.clear()
	if not FileAccess.file_exists(path):
		return 0
	var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(path))
	if parsed is Dictionary:
		for clip: Variant in parsed.get("clips", []):
			if clip is Dictionary and clip.has("id") and clip.has("path"):
				clips[String(clip.id)] = String(clip.path)
	return clips.size()


func catalog() -> Dictionary:
	return _catalog


func category_names() -> Dictionary:
	return _catalog.get("categories", {})


func queue_size() -> int:
	return _queue.size()


func is_speaking() -> bool:
	return not current.is_empty()


func voice_playing() -> bool:
	return _voice != null and _voice.playing


## Take a packet from the bridge's get_radio_events. Old reports are dropped,
## repeats are folded together and the queue never grows past MAX_QUEUE.
func consume(packet: Dictionary) -> void:
	var now_tick := int(packet.get("tick", 0))
	for event: Dictionary in packet.get("events", []):
		var sequence := int(event.get("sequence", 0))
		if sequence <= cursor:
			continue
		cursor = sequence
		stats.received += 1
		var definition: Variant = _events_by_kind.get(int(event.get("kind", -1)))
		if definition == null:
			stats.unknown += 1
			continue
		if not prefs.category_enabled(String(definition.category)):
			stats.filtered += 1
			continue
		var severity: Dictionary = _catalog.severities[definition.severity]
		var age := maxf(0.0, float(now_tick - int(event.get("tick", now_tick))) * TICK_SECONDS)
		if age > float(severity.max_age_seconds):
			stats.stale += 1
			continue
		_accept(definition, severity, event, age)
	cursor = maxi(cursor, int(packet.get("cursor", cursor)))


func _accept(definition: Dictionary, severity: Dictionary, event: Dictionary, age: float) -> void:
	var position := Vector2(float(event.get("x", 0.0)), float(event.get("y", 0.0)))
	var cell := Location.grid_cell(position, map_rect, float(_catalog.get("grid", {}).get("cell_m", 0.0)))
	var key := String(definition.id)
	if String(definition.get("dedupe", "event")) == "sector":
		key += "@" + Location.grid_label(cell)
	# One report per event (and sector) inside its cooldown; the rest only count.
	for queued: Dictionary in _queue:
		if queued.key == key:
			queued.repeats += 1
			stats.deduped += 1
			return
	if _last_accepted.has(key) and _clock - float(_last_accepted[key]) < float(definition.get("cooldown_seconds", 0.0)):
		stats.deduped += 1
		if not current.is_empty() and current.key == key:
			current.repeats += 1
		return
	_last_accepted[key] = _clock
	var item := {
		"key": key, "definition": definition, "rank": int(severity.rank),
		"preempts": bool(severity.get("preempts", false)),
		"expires_at": _clock + float(severity.max_age_seconds) - age,
		"sequence": int(event.sequence), "event": event, "position": position, "cell": cell, "repeats": 0,
	}
	var at := _queue.size()
	for index in range(_queue.size()):
		if int(_queue[index].rank) > item.rank:
			at = index
			break
	_queue.insert(at, item)
	while _queue.size() > MAX_QUEUE:
		# The least important, and among those the newest, goes first.
		_queue.pop_back()
		stats.overflow += 1


func _process(delta: float) -> void:
	advance(delta)


## Moves the announcer's own clock. Real seconds: game speed does not rush speech.
func advance(delta: float) -> void:
	_clock += maxf(delta, 0.0)
	for index in range(_queue.size() - 1, -1, -1):
		if _clock > float(_queue[index].expires_at):
			_queue.remove_at(index)
			stats.expired += 1
	if not current.is_empty():
		_remaining -= delta
		var head_preempts := not _queue.is_empty() and bool(_queue[0].preempts) and int(_queue[0].rank) < int(current.rank)
		if head_preempts:
			stats.preempted += 1
			_finish(true)
		elif _remaining <= 0.0 and not _voice.playing:
			_finish(false)
		else:
			return
	if _gap > 0.0:
		_gap -= delta
		return
	if not _queue.is_empty():
		_start(_queue.pop_front())


func _finish(interrupted: bool) -> void:
	_voice.stop()
	var finished := current
	finished["interrupted"] = interrupted
	current = {}
	_remaining = 0.0
	_gap = 0.0 if interrupted else LINE_GAP_SECONDS
	announcement_finished.emit(finished)


func _start(item: Dictionary) -> void:
	var definition: Dictionary = item.definition
	var event: Dictionary = item.event
	var tokens := _tokens(item)
	var voice_set := String(_catalog.get("faction_voice_sets", {}).get(str(listener_faction), ""))
	if voice_set.is_empty():
		for name: String in _catalog.get("voice_sets", {}):
			if int(_catalog.voice_sets[name].get("faction", -1)) == listener_faction:
				voice_set = name
	var lines: Array = definition.get("lines", {}).get(voice_set, [])
	var text := ""
	var clip := ""
	if not lines.is_empty():
		# Variants take turns. Local only, so it needs no synchronised randomness.
		var turn := int(_variant_turn.get(definition.id, 0))
		_variant_turn[definition.id] = turn + 1
		var variant := turn % lines.size()
		text = fill(String(lines[variant]), tokens)
		if not text.is_empty():
			clip = String(clips.get(clip_id(String(definition.id), voice_set, variant, String(lines[variant]), tokens), ""))
	if text.is_empty():
		text = fill(String(definition.get("fallback", "")), tokens)
	if text.is_empty():
		text = fill(String(definition.get("plain", definition.get("fallback", ""))), {})
	var stream: AudioStream = null
	if prefs.voice_enabled and not clip.is_empty():
		# Generated WAVs can be installed after the editor's import pass.
		# Load their source directly, preserving the original samples.
		if clip.get_extension().to_lower() == "wav" and FileAccess.file_exists(clip):
			stream = AudioStreamWAV.load_from_file(clip)
		elif ResourceLoader.exists(clip):
			stream = load(clip)
	var entry := {
		"id": String(definition.id), "key": item.key, "category": String(definition.category),
		"category_name": String(category_names().get(definition.category, definition.category)),
		"severity": String(definition.severity), "rank": int(item.rank),
		"text": text, "detail": fill(String(definition.get("detail", "")), tokens),
		"voiced": stream != null, "clip": clip, "repeats": int(item.repeats),
		"tick": int(event.get("tick", 0)), "sequence": int(item.sequence),
		"focus": bool(definition.get("focus", false)), "position": item.position,
		"grid": Location.grid_label(item.cell), "count": int(event.get("count", 1)),
	}
	current = entry
	if stream != null:
		_voice.stream = stream
		_voice.play()
		_remaining = maxf(stream.get_length(), 0.5)
		stats.spoken += 1
	else:
		# No recording for this line: a short tone marks it and the text carries it.
		if prefs.voice_enabled and _attention.stream != null:
			_attention.play()
		_remaining = maxf(TEXT_MIN_SECONDS, float(text.split(" ", false).size()) * TEXT_SECONDS_PER_WORD)
		stats.text_only += 1
	history.append(entry)
	while history.size() > MAX_HISTORY:
		history.pop_front()
	history_changed.emit()
	announcement_started.emit(entry)


## Every location word comes from the report's own position and reference.
func _tokens(item: Dictionary) -> Dictionary:
	var event: Dictionary = item.event
	var tokens := {}
	var grid := Location.spoken_grid(item.cell)
	if not grid.is_empty():
		tokens["grid"] = grid
	if bool(event.get("has_reference", false)):
		var reference := Vector2(float(event.reference_x), float(event.reference_y))
		var bearing := Location.bearing_degrees(reference, item.position)
		tokens["bearing"] = Location.spoken_bearing(bearing)
		tokens["cardinal"] = Location.cardinal(bearing)
		tokens["cardinal_code"] = Location.cardinal_code(bearing)
		tokens["range_km"] = Location.spoken_number(Location.range_km(reference, item.position))
	return tokens


## Replaces {token}s. Returns "" if the template needs one that is unknown, so
## a line never goes out with a hole or a made-up location in it.
static func fill(template: String, tokens: Dictionary) -> String:
	var text := template
	for token: String in tokens:
		text = text.replace("{%s}" % token, String(tokens[token]))
	return "" if text.contains("{") else text


## The id the generator gives this line's recording: one clip per variant,
## and one per compass point when the line speaks a direction.
static func clip_id(event_id: String, voice_set: String, variant: int, template: String, tokens: Dictionary) -> String:
	var name := "%s/%s.v%d" % [voice_set, event_id, variant + 1]
	if template.contains("{cardinal}"):
		if not tokens.has("cardinal_code"):
			return ""
		name += "." + String(tokens.cardinal_code)
	return name


## A new match: nothing queued or said carries over.
func clear() -> void:
	if not current.is_empty():
		_finish(true)
	_queue.clear()
	_last_accepted.clear()
	history.clear()
	# The cursor stays: native sequences keep counting across matches, so an
	# old report can never be read again.
	_gap = 0.0
	if _attention != null:
		_attention.stop()
	history_changed.emit()


func _exit_tree() -> void:
	if _voice != null:
		_voice.stop()
		_voice.stream = null
	if _attention != null:
		_attention.stop()
		_attention.stream = null
