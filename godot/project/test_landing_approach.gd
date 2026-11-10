extends SceneTree

const Flight := preload("res://reinforcement_delivery_presentation.gd")
var checks := 0
var failures := 0

func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		push_error(reason)

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var view := Flight.new()
	root.add_child(view)
	await process_frame
	for sign_value in [-1.0, 1.0]:
		var centre := Vector2(-12500.0, 1000.0 * sign_value)
		var runway := centre + Vector2(-55.0, 0.0)
		var axis := Vector2(0.0, sign_value)
		var previous: Dictionary = Flight.landing_pose(runway, axis, 0.0)
		var max_turn := 0.0
		var max_step := 0.0
		var aligned := true
		var on_runway := true
		var descends := true
		for i in range(1, 1001):
			var p := float(i) / 1000.0
			var pose: Dictionary = Flight.landing_pose(runway, axis, p)
			max_turn = maxf(max_turn, absf(wrapf(float(pose.yaw) - float(previous.yaw), -PI, PI)))
			max_step = maxf(max_step, pose.position.distance_to(previous.position))
			descends = descends and pose.position.y <= previous.position.y + 0.001
			if p >= 0.5:
				aligned = aligned and absf(pose.position.x - runway.x) < 0.01 and absf(cos(pose.yaw)) < 0.001
			if p >= 0.8:
				on_runway = on_runway and absf(pose.position.z - runway.y) <= 350.0 and is_equal_approx(pose.position.y, 0.15)
			previous = pose
		check(max_turn < deg_to_rad(1.0), "No instantaneous heading changes: %f degrees" % rad_to_deg(max_turn))
		check(max_step < 3.0, "No position jumps")
		check(aligned, "Final approach is straight along the main runway")
		check(on_runway, "Touchdown and rollout stay on the 45 x 700 m main runway")
		check(descends, "Descent is continuous and never rises or goes underground")
		var end: Vector3 = Flight.landing_pose(runway, axis, 1.0).position
		var near_end: Vector3 = Flight.landing_pose(runway, axis, 0.999).position
		check(end.distance_to(near_end) < 0.02, "Rollout slows to a stop")
		view.sync({"state": 4, "delivery_x": centre.x, "delivery_y": centre.y}, func(_x, _z): return 50.0)
		check(view.transport.position.distance_to(Vector3(runway.x, 50.15, runway.y)) < 0.01, "Actual visual parks on main runway, away from taxiway")
		view.sync({"state": 3, "elapsed_ms": 10000.0, "duration_ms": 20000.0, "delivery_x": centre.x, "delivery_y": centre.y}, func(_x, _z): return 50.0)
		var nose: Vector3 = view.transport.transform.basis.x.normalized()
		check(Vector3(nose.x, 0.0, nose.z).normalized().dot(Vector3(-axis.x, 0.0, -axis.y)) > 0.99, "Mesh nose follows final approach")
	view.queue_free()
	await process_frame
	print("LANDING_APPROACH checks=%d failures=%d" % [checks, failures])
	quit(1 if failures else 0)
