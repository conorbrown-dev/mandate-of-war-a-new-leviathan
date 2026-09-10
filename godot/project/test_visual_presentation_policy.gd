extends SceneTree

const PolicyScript = preload("res://visual_presentation_policy.gd")

func _init() -> void:
	assert(not PolicyScript.use_prototype_wrappers(false, false))
	assert(PolicyScript.use_prototype_wrappers(true, false))
	assert(not PolicyScript.use_prototype_wrappers(true, true))
	assert(not PolicyScript.use_prototype_wrappers(false, true))
	print("VISUAL_PRESENTATION_POLICY checks=4 failures=0")
	quit()
