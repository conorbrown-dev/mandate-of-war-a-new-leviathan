extends Node
## Local UI feedback only. Combat/world audio must use visible native events.

const MANIFEST_PATH := "res://audio/manifest.json"
const VOICE_LIMIT := 4
const GROUP := &"ui_audio"

var enabled := true
var _streams: Dictionary = {}
var _voices: Array[AudioStreamPlayer] = []
var _next_voice := 0
## Hover ticks get their own quiet voice, so sweeping the pointer across a row
## of buttons restarts one short tick instead of cutting off an order cue.
var hover_voice: AudioStreamPlayer

func _ready() -> void:
	add_to_group(GROUP)
	var manifest: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(MANIFEST_PATH))
	for cue: String in manifest.get("cues", {}):
		_streams[cue] = load(String(manifest.cues[cue]))
	for index in range(VOICE_LIMIT):
		var player := AudioStreamPlayer.new()
		player.name = "Voice%d" % index
		player.volume_db = -18.0
		player.bus = "UI"
		add_child(player)
		_voices.append(player)
	var hover: Dictionary = manifest.get("hover", {})
	hover_voice = AudioStreamPlayer.new()
	hover_voice.name = "HoverVoice"
	hover_voice.bus = "UI"
	hover_voice.volume_db = float(hover.get("volume_db", -28.0))
	if hover.has("path"):
		hover_voice.stream = load(String(hover.path))
	add_child(hover_voice)

func play_cue(cue: String) -> bool:
	if not enabled or not _streams.has(cue) or _voices.is_empty():
		return false
	var player := _voices[_next_voice]
	_next_voice = (_next_voice + 1) % VOICE_LIMIT
	player.stream = _streams[cue]
	player.play()
	return true

## The pointer entered an enabled button. Controls call this once per entry
## (Control.mouse_entered), so resting on a button never repeats the tick.
func play_hover() -> bool:
	if not enabled or hover_voice == null or hover_voice.stream == null:
		return false
	hover_voice.play()
	return true


## For shared UI components, which have no reference to the match view.
static func hover(from: Node) -> void:
	if from == null or not from.is_inside_tree():
		return
	var audio := from.get_tree().get_first_node_in_group(GROUP)
	if audio != null:
		audio.play_hover()

func _exit_tree() -> void:
	if hover_voice != null:
		hover_voice.stop()
		hover_voice.stream = null
	for player in _voices:
		player.stop()
		player.stream = null
	_streams.clear()
	_voices.clear()
