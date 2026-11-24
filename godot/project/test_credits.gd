extends SceneTree

# Every entry in res://credits.json names the asset, its author, where it came
# from, its licence and a link; every Sketchfab-sourced model has an entry; and
# the AUDIO & CREDITS panel (F2) shows them all.

const AudioSettingsScript := preload("res://ui/components/audio_settings.gd")

var checks := 0
var failures := 0


func check(condition: bool, message: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		print("VALIDATION FAIL: %s" % message)


func _init() -> void:
	var parsed = JSON.parse_string(FileAccess.get_file_as_string("res://credits.json"))
	var credits: Array = parsed.get("credits", []) if parsed is Dictionary else []
	check(credits.size() > 0, "credits.json lists at least one asset")
	for entry in credits:
		for field in ["asset", "author", "source", "license", "url"]:
			check(not String(entry.get(field, "")).is_empty(), "credit %s has a %s" % [entry.get("asset", "?"), field])
	var definitions: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://visuals/visual_definitions.json"))
	var urls := credits.map(func(entry): return String(entry.get("url", "")))
	var sourced := 0
	for definition in definitions.visual_definitions:
		var source := String(definition.get("source_asset_id", ""))
		if source.begins_with("sketchfab/"):
			sourced += 1
			var uid := source.trim_prefix("sketchfab/").get_slice(" ", 0)
			check(urls.any(func(url): return String(url).ends_with(uid)), "%s has an in-game credit" % definition.visual_id)
	check(sourced == 3, "the three recon models are Sketchfab-sourced (%d)" % sourced)
	var panel: Control = AudioSettingsScript.new()
	root.add_child(panel)
	await process_frame
	var label: RichTextLabel = panel.find_child("AudioCredits", true, false)
	check(label != null, "the AUDIO & CREDITS panel has its credits text")
	if label != null:
		for entry in credits:
			check(label.text.contains(String(entry.url)), "the panel credits %s" % entry.asset)
		check(label.text.contains("SOUND CREDITS"), "the panel keeps its sound credits")
	print("CREDITS checks=%d failures=%d" % [checks, failures])
	quit(1 if failures > 0 else 0)
