extends SceneTree
## Build menu hover, through the real game and real pointer motion.
## - Entering an enabled button plays one quiet hover tick on the UI bus; resting
##   on it does not repeat; a disabled button and disabled UI audio are silent.
## - Hovering a Field Engineer structure or a Strategic Command unit card shows
##   its description from ui/build_descriptions.json.
## - Every native catalog entry has a description, and what each unit is said
##   to engage matches its weapon targets in data/unit_faction_stats.json.

const BuildDescriptions := preload("res://ui/build_descriptions.gd")
const UnitCatalog := preload("res://unit_catalog.gd")

var checks := 0
var failures := 0
var _away := Vector2(4.0, 4.0)


func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		push_error(reason)
		failures += 1


func _initialize() -> void:
	_run.call_deferred()


func _move_to(at: Vector2) -> void:
	var event := InputEventMouseMotion.new()
	event.position = at
	event.global_position = at
	root.push_input(event, true)
	await process_frame


func _centre(control: Control) -> Vector2:
	return control.get_global_transform_with_canvas() * (control.size * 0.5)


# Pointer away, voice quiet, then into `control`: did that one entry tick?
func _hover_ticks(sound: Node, control: Control) -> bool:
	await _move_to(_away)
	sound.hover_voice.stop()
	await _move_to(_centre(control))
	return sound.hover_voice.playing


func _run() -> void:
	var manifest: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://audio/manifest.json"))
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	await process_frame
	var sound: Node = view.ui_audio
	var hover_voice: AudioStreamPlayer = sound.hover_voice
	check(hover_voice != null and hover_voice.bus == "UI" and hover_voice.stream != null and hover_voice.stream.resource_path == String(manifest.hover.path), "hover tick is the manifest's sound on the UI bus")
	check(hover_voice.volume_db < (sound.get_child(0) as AudioStreamPlayer).volume_db, "hover tick is quieter than order cues (%.0f dB)" % hover_voice.volume_db)
	check(String(manifest.hover.path).begins_with("res://audio/kenney_interface/") and FileAccess.file_exists(String(manifest.hover.path)), "hover tick reuses a Kenney Interface sound already in the repo")

	view._on_start_skirmish_pressed()
	# Headless windows never report the pointer entering them; a desktop does.
	root.propagate_notification(Window.NOTIFICATION_WM_MOUSE_ENTER)
	view._set_selected(view._player_engineer_id(), true)
	view._update_hud()
	for _frame in range(3):
		await process_frame
	var catalog: Control = view.command_hud.build_catalog_view
	var entries: Array = catalog.get("_entries")
	check(catalog.visible and entries.size() == 4, "the Field Engineer catalog offers four structures")
	var button: Button = entries[0].get("button") if not entries.is_empty() else null
	if button == null:
		_finish(view)
		return
	check(catalog.description_text() == catalog.DESCRIPTION_HINT, "the description line starts with its hint")

	# One tick per entry into an enabled build button.
	check(await _hover_ticks(sound, button), "entering a build button plays the hover tick")
	sound.hover_voice.stop()
	await _move_to(_centre(button) + Vector2(6.0, 2.0))
	await _move_to(_centre(button) + Vector2(-6.0, -2.0))
	check(not sound.hover_voice.playing, "moving within the button does not repeat the tick")
	check(await _hover_ticks(sound, button), "leaving and entering again ticks again")

	# The hovered structure's description, from the presentation data file.
	var structure_entry: Dictionary = entries[0]
	var expected := String(BuildDescriptions.data().structures[BuildDescriptions.STRUCTURE_KEYS[int(structure_entry.structure_type)]])
	check(not expected.is_empty() and catalog.description_text() == expected, "hovering %s shows its description" % structure_entry.name)
	check(button.tooltip_text.contains(expected), "the button's tooltip carries the description too")
	var second: Button = entries[1].get("button")
	await _move_to(_centre(second))
	check(catalog.description_text() == BuildDescriptions.for_entry(entries[1]) and catalog.description_text() != expected, "moving to the next structure changes the description")
	await _move_to(_away)
	check(catalog.description_text() == catalog.DESCRIPTION_HINT, "leaving the buttons restores the hint")

	# Silent when disabled, or when interface audio is off.
	button.disabled = true
	check(not await _hover_ticks(sound, button), "a disabled button is silent")
	button.disabled = false
	sound.enabled = false
	check(not await _hover_ticks(sound, button), "disabled interface audio is silent")
	sound.enabled = true

	# Strategic Command unit cards.
	view._toggle_strategic_command()
	for _frame in range(3):
		await process_frame
	var panel: Control = view.strategic_command_view
	var cards: GridContainer = panel.get("_cards")
	check(panel.visible and cards.get_child_count() > 0, "Strategic Command shows unit cards")
	check(panel.description_text() == panel.DESCRIPTION_HINT, "the unit description line starts with its hint")
	var holder: Control = cards.get_child(0)
	var card: Control = holder.get_child(0)
	var unit_type := int(String(holder.name).trim_prefix("Card_"))
	var unit_text := BuildDescriptions.for_entry({"type": unit_type, "is_structure": false})
	await _move_to(card.get_global_transform_with_canvas() * Vector2(card.size.x * 0.5, 12.0))
	check(not unit_text.is_empty() and panel.description_text().ends_with(unit_text), "hovering a unit card shows that unit's description")
	check(card.tooltip_text == unit_text, "the card's tooltip carries the description")
	var order: Button = holder.get_node("Order")
	if order.disabled:
		check(not await _hover_ticks(sound, order), "a disabled ORDER button is silent")
	else:
		check(await _hover_ticks(sound, order), "an enabled ORDER button ticks")
	var close: Button = panel.find_child("Close", true, false)
	check(close != null and await _hover_ticks(sound, close), "Strategic Command's CLOSE button ticks")
	var air_tab: Button = panel.find_child("Tab_AIR", true, false)
	check(air_tab != null and await _hover_ticks(sound, air_tab), "the category tabs tick")
	view._close_strategic_command()

	# Every catalog entry is described, and the text agrees with the weapon data.
	for faction in range(3):
		for entry: Dictionary in view.extension.call("get_build_catalog", faction):
			var text := BuildDescriptions.for_entry(entry)
			check(not text.is_empty(), "%s has a description" % entry.get("name", "?"))
			if bool(entry.get("is_structure", false)):
				continue
			var unit: Dictionary = UnitCatalog.units().get(int(entry.type), {})
			var targets: Array = unit.get("weapon", {}).get("targets", [])
			var says := "both ground and air" if targets.size() == 2 else "aircraft only" if targets == ["air"] else "ground targets only" if targets == ["ground"] else "Unarmed"
			check(text.contains(says), "%s says it engages %s (%s)" % [unit.get("type", "?"), says, targets])
			check(text.contains("Paradrop capable") == bool(unit.get("airdrop", false)), "%s mentions paradrop exactly when it can drop" % unit.get("type", "?"))
	_finish(view)


func _finish(view: Node) -> void:
	view.queue_free()
	await process_frame
	var mixer_deadline := Time.get_ticks_msec() + 150
	while Time.get_ticks_msec() < mixer_deadline:
		await process_frame
	print("BUILD_MENU_HOVER checks=%d failures=%d" % [checks, failures])
	quit(1 if failures else 0)
