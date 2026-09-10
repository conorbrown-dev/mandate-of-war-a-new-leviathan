extends SceneTree

const CompatibilityScript = preload("res://visual_pack_compatibility.gd")

func _init() -> void:
	var local_handshake := CompatibilityScript.handshake()
	assert(local_handshake.pack_id == CompatibilityScript.PACK_ID)
	assert(int(local_handshake.pack_version) == CompatibilityScript.PACK_VERSION)
	assert(String(local_handshake.sha256).length() == 64)
	assert(local_handshake == CompatibilityScript.handshake())
	assert(CompatibilityScript.is_compatible(local_handshake, local_handshake.duplicate()))
	var mismatched_hash := local_handshake.duplicate()
	mismatched_hash.sha256 = "0".repeat(64)
	assert(not CompatibilityScript.is_compatible(local_handshake, mismatched_hash))
	var mismatched_version := local_handshake.duplicate()
	mismatched_version.pack_version = CompatibilityScript.PACK_VERSION + 1
	assert(not CompatibilityScript.is_compatible(local_handshake, mismatched_version))
	print("VISUAL_PACK_COMPATIBILITY checks=7 failures=0")
	quit()
