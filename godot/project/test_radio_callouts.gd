extends SceneTree
## Radio callouts: location words, the announcer's queue rules, and the three
## first alerts through a real match. Run with tools/validate.py radio_callouts.

const Location := preload("res://audio/radio/radio_location.gd")
const Announcer := preload("res://audio/radio/radio_announcer.gd")
const SECURED := 0
const LOST := 1
const AIR := 2

var checks := 0
var failures := 0
var _sequence := 0

func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		push_error(reason)
		failures += 1

func _initialize() -> void:
	_run.call_deferred()

func report(kind: int, at: Vector2, tick := 0, reference: Variant = Vector2.ZERO) -> Dictionary:
	_sequence += 1
	var event := {"sequence": _sequence, "tick": tick, "kind": kind, "x": at.x, "y": at.y, "count": 1, "subject": 0,
		"has_reference": reference != null, "reference_x": 0.0, "reference_y": 0.0}
	if reference != null:
		event.reference_x = reference.x
		event.reference_y = reference.y
	return event

func packet(events: Array, tick := 0) -> Dictionary:
	return {"cursor": _sequence, "tick": tick, "events": events}

func fresh_announcer() -> Node:
	preload("res://audio/audio_mix.gd").new().setup()
	var radio: Node = Announcer.new()
	root.add_child(radio)
	# After entering the tree: a node with _process turns processing back on
	# when it enters, so the announcer must be stopped here to run on virtual time.
	radio.set_process(false)
	radio.map_rect = Rect2(-14000, -14000, 28000, 28000)
	radio.clips.clear()
	return radio

func _check_location_words() -> void:
	var origin := Vector2.ZERO
	check(Location.bearing_degrees(origin, Vector2(0, -1000)) == 0, "north is minus y and bearing 000")
	check(Location.bearing_degrees(origin, Vector2(1000, 0)) == 90, "east is plus x and bearing 090")
	check(Location.bearing_degrees(origin, Vector2(0, 1000)) == 180, "south is bearing 180")
	check(Location.bearing_degrees(origin, Vector2(-1000, 0)) == 270, "west is bearing 270")
	check(Location.bearing_degrees(Vector2(500, 500), Vector2(1500, -500)) == 45, "bearing is taken from the reference point, not the map origin")
	check(Location.spoken_bearing(45) == "zero-four-five", "bearings are three spoken digits")
	check(Location.spoken_bearing(359) == "three-five-niner" and Location.spoken_bearing(0) == "zero-zero-zero", "bearing digits use radio niner and keep leading zeros")
	check(Location.cardinal(45) == "northeast" and Location.cardinal(22) == "north" and Location.cardinal(23) == "northeast", "cardinals switch at the half point")
	check(Location.cardinal(338) == "north" and Location.cardinal(337) == "northwest" and Location.cardinal(180) == "south", "cardinals wrap around north")
	check(Location.cardinal_code(270) == "w" and Location.cardinal_code(135) == "se", "clip codes match the cardinal")
	check(Location.clock_direction(origin, 0, Vector2(1000, 0)) == 3, "a contact due east of a north-facing unit is at three o'clock")
	check(Location.clock_direction(origin, 90, Vector2(1000, 0)) == 12, "the same contact is at twelve for a unit facing east")
	check(Location.clock_direction(origin, 90, Vector2(0, -1000)) == 9 and Location.spoken_clock(9) == "nine o'clock", "clock direction follows the unit's facing")
	check(Location.spoken_number(80) == "eighty" and Location.spoken_number(7) == "seven" and Location.spoken_number(115) == "one hundred fifteen" and Location.spoken_number(42) == "forty-two", "numbers are spoken as words")
	check(Location.range_km(origin, Vector2(8000, 0)) == 8 and Location.range_km(origin, Vector2(100, 0)) == 1, "range is whole kilometres, at least one")
	var map := Rect2(-14000, -14000, 28000, 28000)
	var cell := Location.grid_cell(Vector2(-14000 + 3 * 2000 + 10, -14000 + 6 * 2000 + 10), map, 2000.0)
	check(Location.spoken_grid(cell) == "Delta-Seven" and Location.grid_label(cell) == "D7", "fourth column, seventh row is Delta-Seven")
	check(Location.grid_label(Location.grid_cell(Vector2(-14000, -14000), map, 2000.0)) == "A1", "the grid starts at the north-west corner")
	check(Location.grid_label(Location.grid_cell(Vector2(14000, 14000), map, 2000.0)) == "N14", "the far corner is the last cell, not one past it")
	check(Location.grid_cell(Vector2(20000, 0), map, 2000.0).is_empty() and Location.spoken_grid({}) == "", "a position off the map has no grid reference")

func _check_announcer_rules() -> void:
	var radio := fresh_announcer()
	var started: Array[Dictionary] = []
	radio.announcement_started.connect(func(entry: Dictionary) -> void: started.append(entry))
	var north_east := Vector2(6000, -6000)
	# Severity decides the order, whatever order the reports arrived in.
	radio.consume(packet([report(SECURED, Vector2(100, 100)), report(LOST, Vector2(5000, 100)), report(AIR, north_east)]))
	check(radio.queue_size() == 3 and not radio.is_speaking(), "reports wait for the announcer's turn")
	var overlap := false
	for _step in range(600):
		radio.advance(0.05)
		overlap = overlap or (radio.voice_playing() and radio.current.is_empty())
	check(started.size() == 3, "all three reports are delivered")
	if started.size() == 3:
		check(started[0].id == "air.hostile_inbound" and started[1].id == "objective.fuel_depot_lost" and started[2].id == "objective.fuel_depot_secured", "critical, then urgent, then operational")
		check(started[0].text.contains("northeast") and not started[0].text.contains("{"), "the air report names the direction it was seen in")
		check(started[0].detail.contains("zero-four-five") and started[0].detail.contains("eight kilometers"), "its detail gives the spoken bearing and range from command")
		check(started[0].detail.contains(Location.spoken_grid(Location.grid_cell(north_east, radio.map_rect, 2000.0))), "and the grid cell of the sighting")
		check(not started[0].voiced and not started[0].text.is_empty(), "with no recording the report is still delivered as text")
		check(started[0].focus and started[0].position == north_east, "the report keeps the reported location for the map jump")
	check(not overlap and radio.stats.text_only == 3 and radio.stats.spoken == 0, "text-only lines start no speech")
	check(radio.history.size() == 3 and not radio.is_speaking(), "history keeps what was said after it ends")

	# A recorded clip plays on the one voice, never over another line.
	radio.clear()
	started.clear()
	radio.clips["elite_command/objective.fuel_depot_secured.v1"] = "res://audio/edited/ak_single.wav"
	radio.clips["elite_command/objective.fuel_depot_lost.v1"] = "res://audio/edited/smg_burst.wav"
	radio.consume(packet([report(SECURED, Vector2(100, 100)), report(LOST, Vector2(5000, 100))]))
	radio.advance(0.05)
	check(radio.current.get("id", "") == "objective.fuel_depot_lost" and radio.current.voiced and radio.voice_playing(), "a recorded line is spoken")
	check(radio.get_node("RadioVoice").bus == "Voice", "speech has its own volume channel")
	check(AudioServer.get_bus_effect_count(AudioServer.get_bus_index("Voice")) == 0, "voice bus has no processing effects")
	check(radio.get_node_or_null("RadioStatic") == null and radio.get_node_or_null("RadioSquelch") == null, "voiceovers have no static or squelch players")
	var players := 0
	for child in radio.get_children():
		players += int(child is AudioStreamPlayer and child.playing)
	check(players == 1 and radio.queue_size() == 1, "one voice speaks while the next line waits")
	# A critical report cuts in on a lower one.
	radio.consume(packet([report(AIR, north_east)]))
	radio.advance(0.05)
	check(radio.current.get("id", "") == "air.hostile_inbound" and radio.stats.preempted == 1, "a critical alert preempts the line being spoken")
	check(not radio.voice_playing(), "the interrupted speech stops at once")
	check(radio.history[0].get("interrupted", false), "the cut-off line stays in the log")
	# An operational report never cuts in on a critical one.
	radio.consume(packet([report(LOST, Vector2(9000, 9000))]))
	radio.advance(0.05)
	check(radio.current.get("id", "") == "air.hostile_inbound", "lower alerts wait for a critical one to finish")

	# Voice off: same reports, text only.
	radio.clear()
	started.clear()
	radio.prefs.voice_enabled = false
	radio.consume(packet([report(SECURED, Vector2(100, 100))]))
	radio.advance(0.05)
	check(started.size() == 1 and not started[0].voiced and not radio.voice_playing() and not radio.get_node("RadioAttention").playing, "with spoken alerts off nothing plays and the text remains")
	radio.prefs.voice_enabled = true

	# Flood: thousands of identical reports are one line; many different ones stay bounded.
	radio.clear()
	started.clear()
	var before: int = radio.stats.deduped
	var flood := []
	for index in range(2000):
		flood.append(report(AIR, north_east + Vector2(index, 0)))
	radio.consume(packet(flood))
	check(radio.queue_size() == 1 and radio.stats.deduped - before == 1999, "2,000 identical air reports fold into one")
	radio.advance(0.05)
	radio.consume(packet([report(AIR, north_east)]))
	check(radio.queue_size() == 0 and radio.current.repeats == 2000, "a repeat inside the cooldown is counted, not queued")
	var spread := []
	for index in range(60):
		spread.append(report(SECURED, Vector2(-13000 + float(index % 12) * 2000.0, -13000 + float(index / 12) * 2000.0)))
	radio.consume(packet(spread))
	check(radio.queue_size() == radio.MAX_QUEUE and radio.stats.overflow == 60 - radio.MAX_QUEUE, "the queue is bounded however many reports arrive")
	# Same sector twice is one report; a different sector is another.
	radio.clear()
	radio.consume(packet([report(SECURED, Vector2(100, 100)), report(SECURED, Vector2(300, 300)), report(SECURED, Vector2(9000, 9000))]))
	check(radio.queue_size() == 2, "depot reports are folded per map sector")

	# Old news is never read out.
	radio.clear()
	started.clear()
	var stale_before: int = radio.stats.stale
	radio.consume(packet([report(AIR, north_east, 0)], 20 * 120))
	check(radio.queue_size() == 0 and radio.stats.stale == stale_before + 1, "a two-minute-old tactical report is dropped")
	radio.consume(packet([report(LOST, Vector2(100, 100), 0)], 0))
	var expired_before: int = radio.stats.expired
	radio._queue[0].expires_at = radio._clock + 1.0
	radio.current = {"rank": 0, "key": "busy", "sequence": -1}
	radio._remaining = 5.0
	radio.advance(2.0)
	check(radio.queue_size() == 0 and radio.stats.expired == expired_before + 1 and started.is_empty(), "a report that waited too long behind another is dropped, not spoken late")
	radio.current = {}

	# No reference point: never a made-up direction.
	radio.clear()
	started.clear()
	radio.consume(packet([report(AIR, north_east, 0, null)]))
	radio.advance(0.05)
	check(started.size() == 1 and started[0].text == "Hostile aircraft inbound." and not started[0].detail.contains("Bearing"), "without a reference point no direction is spoken")

	# Preferences and unknown content.
	radio.clear()
	started.clear()
	radio.prefs.set_category("air_defense", false)
	radio.consume(packet([report(AIR, north_east), report(99, north_east), report(SECURED, Vector2(100, 100))]))
	radio.advance(0.05)
	check(started.size() == 1 and started[0].id == "objective.fuel_depot_secured", "a switched-off category and an unknown event id are skipped without error")
	radio.prefs.set_category("air_defense", true)
	check(radio.use_catalog({"schema_version": 2}) != "" and radio.catalog().has("events"), "an unsupported catalog is refused and the current one kept")
	check(radio.load_catalog("res://audio/radio/nope.json") != "" and radio.load_voice_manifest("res://audio/radio/nope.json") == 0, "missing catalog or voice pack files are reported, not fatal")
	# Every faction has its own wording for the live alerts.
	var texts := {}
	for faction in range(3):
		radio.clear()
		started.clear()
		radio.listener_faction = faction
		radio.consume(packet([report(AIR, north_east)]))
		radio.advance(0.05)
		texts[started[0].text] = true
	check(texts.size() == 3, "each faction's command voice words the alert differently")
	radio.listener_faction = 0
	for event: Dictionary in radio.catalog().events:
		if String(event.get("status", "")) == "parked":
			check(not event.has("native_kind") and event.has("needs"), "parked alert %s is not wired to an event and says what it needs" % event.id)
	radio.queue_free()

func _run_ticks(view: Node, limit: int, done: Callable) -> bool:
	for _tick in range(limit):
		view._process(0.05)
		if done.call():
			return true
	return false

func _history_count(radio: Node, id: String) -> int:
	var count := 0
	for entry: Dictionary in radio.history:
		count += int(entry.id == id)
	return count

func _check_real_match() -> void:
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	view._on_start_skirmish_pressed()
	view.set_process(false)
	view.extension.call("skirmish_set_ai_enabled", false)
	var radio: Node = view.radio_announcer
	var feed: Control = view.radio_feed
	radio.set_process(false)
	# The view loads whatever voice pack is installed; these checks are about
	# text, so they must not depend on whether clips have been generated.
	radio.clips.clear()
	check(radio != null and feed != null and radio.listener_faction == 0, "the match view has a radio for the player")
	var engineer := int(view._player_engineer_id())
	var home: PackedFloat32Array = view.extension.call("get_unit_position", engineer)
	var depots: Array = view.extension.call("fuel_supply_state", 0).depots
	check(not depots.is_empty(), "the scenario has fuel depots")
	var depot: Dictionary = depots[0]
	for candidate: Dictionary in depots:
		if Vector2(candidate.x, candidate.y).distance_to(Vector2(home[0], home[1])) < Vector2(depot.x, depot.y).distance_to(Vector2(home[0], home[1])):
			depot = candidate
	var site := Vector2(depot.x, depot.y)
	check(view.extension.call("economy_assign_fuel_depot", int(depot.id), 1), "the enemy holds the depot at the start")
	for _tick in range(20):
		view._process(0.05)
		radio.advance(0.05)
	check(radio.history.is_empty(), "holding a depot from the start is not news")

	# The player's unit drives in and takes it.
	var captor := int(view.extension.call("create_unit_with_type", site.x - 70.0, site.y, 13, 0))
	check(view.extension.call("issue_harvest_commands", PackedInt32Array([captor]), 0, site.x, site.y) == 1, "capture order accepted")
	var secured := _run_ticks(view, 600, func() -> bool:
		radio.advance(0.05)
		return _history_count(radio, "objective.fuel_depot_secured") > 0)
	check(secured, "taking the depot is announced through the real match loop")
	if secured:
		var entry: Dictionary = radio.history[-1]
		check(entry.text == "Command, fuel depot secured. Logistics personnel moving in.", "the player's faction voice words the report")
		check(entry.position.distance_to(site) < 1.0 and not String(entry.grid).is_empty(), "the report carries the depot's own position and grid")
		check(feed.subtitle_panel.visible and feed.text_label.text == entry.text, "the report is on screen as text while it plays")
		check(feed.caption_label.text.contains("OBJECTIVES") and feed.caption_label.text.contains("TEXT ONLY"), "the text names the category and that no recording exists")
		view.camera_target = Vector3.ZERO
		feed.subtitle_button.pressed.emit()
		check(Vector2(view.camera_target.x, view.camera_target.z).distance_to(site) < 1.0, "clicking the report moves the camera to the depot")
	for _tick in range(200):
		view._process(0.05)
		radio.advance(0.05)
	check(_history_count(radio, "objective.fuel_depot_secured") == 1, "one capture is one announcement, not one a tick")
	check(not feed.subtitle_panel.visible, "the text leaves when the line ends")

	# The enemy takes it back: the player hears the loss.
	var raider := int(view.extension.call("create_unit_with_type", site.x + 70.0, site.y, 15, 1))
	view.extension.call("issue_harvest_commands", PackedInt32Array([raider]), 1, site.x, site.y)
	var lost := _run_ticks(view, 600, func() -> bool:
		radio.advance(0.05)
		return _history_count(radio, "objective.fuel_depot_lost") > 0)
	check(lost, "losing the depot is announced")
	check(_history_count(radio, "objective.fuel_depot_secured") == 1, "the enemy's capture is not reported to the player as a gain")
	check(view.extension.call("get_radio_events", 0, 1).events.size() == 2, "the enemy side has its own two reports")
	check(view.extension.call("get_radio_events", 0, -1).is_empty() and view.extension.call("get_radio_events", -1, 0).is_empty(), "invalid listener or cursor is refused at the bridge")

	# The radio log and its switches.
	feed.toggle_log()
	check(feed.log_panel.visible and feed.log_rows.get_child_count() == radio.history.size(), "F3 lists the reports so far")
	view.camera_target = Vector3.ZERO
	feed.log_rows.get_child(0).pressed.emit()
	check(Vector2(view.camera_target.x, view.camera_target.z).distance_to(site) < 1.0, "a log entry jumps to its location")
	feed.toggle_log()
	var panel: Control = view.audio_settings
	check(panel.sliders.has("Voice") and panel.radio_toggles.has("voice") and panel.radio_toggles.has("subtitles") and panel.radio_toggles.has("category:air_defense"), "audio settings offer voice volume, spoken alerts, alert text and categories")
	panel.radio_toggles["subtitles"].button_pressed = false
	check(not radio.prefs.subtitles_enabled, "the alert text switch changes the radio's preference")

	# Hostile aircraft: an enemy paradrop flight crosses the player's sky.
	var spotter := int(view.extension.call("create_unit_with_type", home[0] + 120.0, home[1], 15, 1))
	for _tick in range(10):
		view._process(0.05)
	check(String(view.extension.call("order_paradrop", 1, 15, home[0] + 120.0, home[1])).is_empty(), "the enemy orders a real paradrop onto ground it watches")
	var air_before := _history_count(radio, "air.hostile_inbound")
	var warned := _run_ticks(view, 4000, func() -> bool:
		radio.advance(0.05)
		return _history_count(radio, "air.hostile_inbound") > air_before)
	check(spotter > 0 and warned, "the hostile transport is called out when the player's units see it")
	if warned:
		var air: Dictionary = radio.history[-1]
		check(air.severity == "critical" and air.text.begins_with("Overwatch has bandits confirmed, "), "it is a critical air-defense alert in the faction's words")
		check(view.extension.call("can_see", 0, air.position.x, air.position.y, true), "the reported position is sky the player is watching")
		check(not feed.subtitle_panel.visible and radio.is_speaking(), "with alert text off the line plays without its text")
		check(radio.history.has(air), "and is still in the radio log")
	for _tick in range(200):
		view._process(0.05)
		radio.advance(0.05)
	check(_history_count(radio, "air.hostile_inbound") == air_before + 1, "one raid is one alert while it stays in sight")
	panel.radio_toggles["subtitles"].button_pressed = true

	var screenshot := OS.get_environment("RTS_RADIO_SCREENSHOT")
	if not screenshot.is_empty() and DisplayServer.get_name() != "headless":
		radio.clear()
		view.camera_target = Vector3(site.x, view._terrain_height_at(site.x, site.y), site.y)
		view.extension.call("economy_assign_fuel_depot", int(depot.id), 1)
		var again := int(view.extension.call("create_unit_with_type", site.x - 70.0, site.y, 13, 0))
		view.extension.call("issue_harvest_commands", PackedInt32Array([again]), 0, site.x, site.y)
		_run_ticks(view, 600, func() -> bool:
			radio.advance(0.05)
			return radio.is_speaking())
		feed.toggle_log()
		for _frame in range(5):
			await process_frame
		RenderingServer.force_draw()
		await RenderingServer.frame_post_draw
		root.get_texture().get_image().save_png(screenshot)

	# A new match starts with a silent, empty radio.
	radio.clear()
	check(radio.history.is_empty() and radio.queue_size() == 0 and not radio.is_speaking(), "a match reset clears the radio")
	view._process(0.05)
	radio.advance(0.05)
	check(radio.history.is_empty() and radio.queue_size() == 0, "and earlier reports are not read out again")
	view.queue_free()
	await process_frame

func _run() -> void:
	_check_location_words()
	_check_announcer_rules()
	await _check_real_match()
	var mixer_deadline := Time.get_ticks_msec() + 150
	while Time.get_ticks_msec() < mixer_deadline:
		await process_frame
	print("RADIO_CALLOUTS checks=%d failures=%d" % [checks, failures])
	quit(1 if failures else 0)
