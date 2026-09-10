class_name VisualPackCompatibility
extends RefCounted

const PACK_ID := "mandate_of_war.prototype_visuals"
const PACK_VERSION := 1

static func definition_sha256(definitions_path := "res://visuals/visual_definitions.json") -> String:
	var file := FileAccess.open(definitions_path, FileAccess.READ)
	if file == null:
		return ""
	var hashing := HashingContext.new()
	hashing.start(HashingContext.HASH_SHA256)
	hashing.update(file.get_buffer(file.get_length()))
	return hashing.finish().hex_encode()

static func handshake(definitions_path := "res://visuals/visual_definitions.json") -> Dictionary:
	return {"pack_id": PACK_ID, "pack_version": PACK_VERSION, "sha256": definition_sha256(definitions_path)}

static func is_compatible(local_handshake: Dictionary, remote_handshake: Dictionary) -> bool:
	return local_handshake.get("pack_id", "") == remote_handshake.get("pack_id", "") \
		and int(local_handshake.get("pack_version", -1)) == int(remote_handshake.get("pack_version", -2)) \
		and local_handshake.get("sha256", "") == remote_handshake.get("sha256", "") \
		and not String(local_handshake.get("sha256", "")).is_empty()
