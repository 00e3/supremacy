#include "includes.h"

Movement g_movement{ };;

void Movement::JumpRelated() {
	if (g_cl.m_local->m_MoveType() == MOVETYPE_NOCLIP)
		return;

	if ((g_cl.m_cmd->m_buttons & IN_JUMP) && !(g_cl.m_flags & FL_ONGROUND)) {
		// bhop.
		if (g_menu.main.misc.bunnyhop.get())
			g_cl.m_cmd->m_buttons &= ~IN_JUMP;
	}
}

void Movement::Strafe() {
	vec3_t velocity;
	float  delta, abs_delta, velocity_delta, correct;

	// don't strafe while noclipping or on ladders..
	if (g_cl.m_local->m_MoveType() == MOVETYPE_NOCLIP || g_cl.m_local->m_MoveType() == MOVETYPE_LADDER)
		return;

	// get networked velocity ( maybe absvelocity better here? ).
	// meh, should be predicted anyway? ill see.
	velocity = g_cl.m_local->m_vecAbsVelocity();

	// get the velocity len2d ( speed ).
	m_speed = velocity.length_2d();

	// compute the ideal strafe angle for our velocity.
	m_ideal = (m_speed > 0.f) ? math::rad_to_deg(std::asin(15.f / m_speed)) : 90.f;
	m_ideal2 = (m_speed > 0.f) ? math::rad_to_deg(std::asin(30.f / m_speed)) : 90.f;

	// some additional sanity.
	math::clamp(m_ideal, 0.f, 90.f);
	math::clamp(m_ideal2, 0.f, 90.f);

	// save entity bounds ( used much in circle-strafer ).
	m_mins = g_cl.m_local->m_vecMins();
	m_maxs = g_cl.m_local->m_vecMaxs();

	// save our origin
	m_origin = g_cl.m_local->m_vecOrigin();

	// disable strafing while pressing shift.
	if ((g_cl.m_buttons & IN_SPEED) || (g_cl.m_flags & FL_ONGROUND))
		return;

	// for changing direction.
	// we want to change strafe direction every call.
	m_switch_value *= -1.f;

	// for allign strafer.
	++m_strafe_index;

	if (g_cl.m_pressing_move && g_menu.main.misc.autostrafe.get()) {
		// took this idea from stacker, thank u !!!!
		enum EDirections {
			FORWARDS = 0,
			BACKWARDS = 180,
			LEFT = 90,
			RIGHT = -90,
			BACK_LEFT = 135,
			BACK_RIGHT = -135
		};

		float wish_dir{ };

		// get our key presses.
		bool holding_w = g_cl.m_buttons & IN_FORWARD;
		bool holding_a = g_cl.m_buttons & IN_MOVELEFT;
		bool holding_s = g_cl.m_buttons & IN_BACK;
		bool holding_d = g_cl.m_buttons & IN_MOVERIGHT;

		// move in the appropriate direction.
		if (holding_w) {
			//	forward left
			if (holding_a) {
				wish_dir += (EDirections::LEFT / 2);
			}
			//	forward right
			else if (holding_d) {
				wish_dir += (EDirections::RIGHT / 2);
			}
			//	forward
			else {
				wish_dir += EDirections::FORWARDS;
			}
		}
		else if (holding_s) {
			//	back left
			if (holding_a) {
				wish_dir += EDirections::BACK_LEFT;
			}
			//	back right
			else if (holding_d) {
				wish_dir += EDirections::BACK_RIGHT;
			}
			//	back
			else {
				wish_dir += EDirections::BACKWARDS;
			}

			g_cl.m_cmd->m_forward_move = 0;
		}
		else if (holding_a) {
			//	left
			wish_dir += EDirections::LEFT;
		}
		else if (holding_d) {
			//	right
			wish_dir += EDirections::RIGHT;
		}

		g_cl.m_strafe_angles.y += math::NormalizeYaw(wish_dir);
	}

	// cancel out any forwardmove values.
	g_cl.m_cmd->m_forward_move = 0.f;

	if (!g_menu.main.misc.autostrafe.get())
		return;

	// get our viewangle change.
	delta = math::NormalizedAngle(g_cl.m_strafe_angles.y - m_old_yaw);

	// convert to absolute change.
	abs_delta = std::abs(delta);

	// save old yaw for next call.
	m_circle_yaw = m_old_yaw = g_cl.m_strafe_angles.y;

	// set strafe direction based on mouse direction change.
	if (delta > 0.f)
		g_cl.m_cmd->m_side_move = -450.f;

	else if (delta < 0.f)
		g_cl.m_cmd->m_side_move = 450.f;

	// we can accelerate more, because we strafed less then needed
	// or we got of track and need to be retracked.
	if (abs_delta <= m_ideal || abs_delta >= 30.f) {
		// compute angle of the direction we are traveling in.
		ang_t velocity_angle;
		math::VectorAngles(velocity, velocity_angle);

		// get the delta between our direction and where we are looking at.
		velocity_delta = math::NormalizeYaw(g_cl.m_strafe_angles.y - velocity_angle.y);

		// correct our strafe amongst the path of a circle.
		correct = m_ideal;

		if (velocity_delta <= correct || m_speed <= 15.f) {
			// not moving mouse, switch strafe every tick.
			if (-correct <= velocity_delta || m_speed <= 15.f) {
				g_cl.m_strafe_angles.y += (m_ideal * m_switch_value);
				g_cl.m_cmd->m_side_move = 450.f * m_switch_value;
			}

			else {
				g_cl.m_strafe_angles.y = velocity_angle.y - correct;
				g_cl.m_cmd->m_side_move = 450.f;
			}
		}

		else {
			g_cl.m_strafe_angles.y = velocity_angle.y + correct;
			g_cl.m_cmd->m_side_move = -450.f;
		}
	}
}

void Movement::DoPrespeed() {
	float   mod, min, max, step, strafe, time, angle;
	vec3_t  plane;

	// min and max values are based on 128 ticks.
	mod = g_csgo.m_globals->m_interval * 128.f;

	// scale min and max based on tickrate.
	min = 2.25f * mod;
	max = 5.f * mod;

	// compute ideal strafe angle for moving in a circle.
	strafe = m_ideal * 2.f;

	// clamp ideal strafe circle value to min and max step.
	math::clamp(strafe, min, max);

	// calculate time.
	time = 320.f / m_speed;

	// clamp time.
	math::clamp(time, 0.35f, 1.f);

	// init step.
	step = strafe;

	while (true) {
		// if we will not collide with an object or we wont accelerate from such a big step anymore then stop.
		if (!WillCollide(time, step) || max <= step)
			break;

		// if we will collide with an object with the current strafe step then increment step to prevent a collision.
		step += 0.2f;
	}

	if (step > max) {
		// reset step.
		step = strafe;

		while (true) {
			// if we will not collide with an object or we wont accelerate from such a big step anymore then stop.
			if (!WillCollide(time, step) || step <= -min)
				break;

			// if we will collide with an object with the current strafe step decrement step to prevent a collision.
			step -= 0.2f;
		}

		if (step < -min) {
			if (GetClosestPlane(plane)) {
				// grab the closest object normal
				// compute the angle of the normal
				// and push us away from the object.
				angle = math::rad_to_deg(std::atan2(plane.y, plane.x));
				step = -math::NormalizedAngle(m_circle_yaw - angle) * 0.1f;
			}
		}

		else
			step -= 0.2f;
	}

	else
		step += 0.2f;

	// add the computed step to the steps of the previous circle iterations.
	m_circle_yaw = math::NormalizedAngle(m_circle_yaw + step);

	// apply data to usercmd.
	g_cl.m_cmd->m_view_angles.y = m_circle_yaw;
	g_cl.m_cmd->m_side_move = (step >= 0.f) ? -450.f : 450.f;
}

bool Movement::GetClosestPlane(vec3_t& plane) {
	CGameTrace            trace;
	CTraceFilterWorldOnly filter;
	vec3_t                start{ m_origin };
	float                 smallest{ 1.f };
	const float		      dist{ 75.f };

	// trace around us in a circle
	for (float step{ }; step <= math::pi_2; step += (math::pi / 10.f)) {
		// extend endpoint x units.
		vec3_t end = start;
		end.x += std::cos(step) * dist;
		end.y += std::sin(step) * dist;

		g_csgo.m_engine_trace->TraceRay(Ray(start, end, m_mins, m_maxs), CONTENTS_SOLID, &filter, &trace);

		// we found an object closer, then the previouly found object.
		if (trace.m_fraction < smallest) {
			// save the normal of the object.
			plane = trace.m_plane.m_normal;
			smallest = trace.m_fraction;
		}
	}

	// did we find any valid object?
	return smallest != 1.f && plane.z < 0.1f;
}

bool Movement::WillCollide(float time, float change) {
	struct PredictionData_t {
		vec3_t start;
		vec3_t end;
		vec3_t velocity;
		float  direction;
		bool   ground;
		float  predicted;
	};

	PredictionData_t      data;
	CGameTrace            trace;
	CTraceFilterWorldOnly filter;

	// set base data.
	data.ground = g_cl.m_flags & FL_ONGROUND;
	data.start = m_origin;
	data.end = m_origin;
	data.velocity = g_cl.m_local->m_vecVelocity();
	data.direction = math::rad_to_deg(std::atan2(data.velocity.y, data.velocity.x));

	for (data.predicted = 0.f; data.predicted < time; data.predicted += g_csgo.m_globals->m_interval) {
		// predict movement direction by adding the direction change.
		// make sure to normalize it, in case we go over the -180/180 turning point.
		data.direction = math::NormalizedAngle(data.direction + change);

		// pythagoras.
		float hyp = data.velocity.length_2d();

		// adjust velocity for new direction.
		data.velocity.x = std::cos(math::deg_to_rad(data.direction)) * hyp;
		data.velocity.y = std::sin(math::deg_to_rad(data.direction)) * hyp;

		// assume we bhop, set upwards impulse.
		if (data.ground)
			data.velocity.z = g_csgo.sv_jump_impulse->GetFloat();

		else
			data.velocity.z -= g_csgo.sv_gravity->GetFloat() * g_csgo.m_globals->m_interval;

		// we adjusted the velocity for our new direction.
		// see if we can move in this direction, predict our new origin if we were to travel at this velocity.
		data.end += (data.velocity * g_csgo.m_globals->m_interval);

		// trace
		g_csgo.m_engine_trace->TraceRay(Ray(data.start, data.end, m_mins, m_maxs), MASK_PLAYERSOLID, &filter, &trace);

		// check if we hit any objects.
		if (trace.m_fraction != 1.f && trace.m_plane.m_normal.z <= 0.9f)
			return true;
		if (trace.m_startsolid || trace.m_allsolid)
			return true;

		// adjust start and end point.
		data.start = data.end = trace.m_endpos;

		// move endpoint 2 units down, and re-trace.
		// do this to check if we are on th floor.
		g_csgo.m_engine_trace->TraceRay(Ray(data.start, data.end - vec3_t{ 0.f, 0.f, 2.f }, m_mins, m_maxs), MASK_PLAYERSOLID, &filter, &trace);

		// see if we moved the player into the ground for the next iteration.
		data.ground = trace.hit() && trace.m_plane.m_normal.z > 0.7f;
	}

	// the entire loop has ran
	// we did not hit shit.
	return false;
}

void Movement::MoonWalk(CUserCmd* cmd) {
	if (g_cl.m_local->m_MoveType() == MOVETYPE_LADDER)
		return;

	// slide walk
	g_cl.m_cmd->m_buttons |= IN_BULLRUSH;

	if (g_menu.main.antiaim.slide_type.get() == 1) {
		if (cmd->m_side_move < 0.f)
		{
			cmd->m_buttons |= IN_MOVERIGHT;
			cmd->m_buttons &= ~IN_MOVELEFT;
		}

		if (cmd->m_side_move > 0.f)
		{
			cmd->m_buttons |= IN_MOVELEFT;
			cmd->m_buttons &= ~IN_MOVERIGHT;
		}

		if (cmd->m_forward_move > 0.f)
		{
			cmd->m_buttons |= IN_BACK;
			cmd->m_buttons &= ~IN_FORWARD;
		}

		if (cmd->m_forward_move < 0.f)
		{
			cmd->m_buttons |= IN_FORWARD;
			cmd->m_buttons &= ~IN_BACK;
		}
	}

}

void Movement::FixMove(CUserCmd* cmd, const ang_t& wish_angles) {

	vec3_t  move, dir;
	float   delta, len;
	ang_t   move_angle;

	// roll nospread fix.
	if (!(g_cl.m_flags & FL_ONGROUND) && cmd->m_view_angles.z != 0.f)
		cmd->m_side_move = 0.f;

	// convert movement to vector.
	move = { cmd->m_forward_move, cmd->m_side_move, 0.f };

	// get move length and ensure we're using a unit vector ( vector with length of 1 ).
	len = move.normalize();
	if (!len)
		return;

	// convert move to an angle.
	math::VectorAngles(move, move_angle);

	// calculate yaw delta.
	delta = (cmd->m_view_angles.y - wish_angles.y);

	// accumulate yaw delta.
	move_angle.y += delta;

	// calculate our new move direction.
	// dir = move_angle_forward * move_length
	math::AngleVectors(move_angle, &dir);

	// scale to og movement.
	dir *= len;

	// strip old flags.
	g_cl.m_cmd->m_buttons &= ~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT);

	// fix ladder and noclip.
	if (g_cl.m_local->m_MoveType() == MOVETYPE_LADDER) {
		// invert directon for up and down.
		if (cmd->m_view_angles.x >= 45.f && wish_angles.x < 45.f && std::abs(delta) <= 65.f)
			dir.x = -dir.x;

		// write to movement.
		cmd->m_forward_move = dir.x;
		cmd->m_side_move = dir.y;

		// set new button flags.
		if (cmd->m_forward_move > 200.f)
			cmd->m_buttons |= IN_FORWARD;

		else if (cmd->m_forward_move < -200.f)
			cmd->m_buttons |= IN_BACK;

		if (cmd->m_side_move > 200.f)
			cmd->m_buttons |= IN_MOVERIGHT;

		else if (cmd->m_side_move < -200.f)
			cmd->m_buttons |= IN_MOVELEFT;
	}

	// we are moving normally.
	else {
		// we must do this for pitch angles that are out of bounds.
		if (cmd->m_view_angles.x < -90.f || cmd->m_view_angles.x > 90.f)
			dir.x = -dir.x;

		// set move.
		cmd->m_forward_move = dir.x;
		cmd->m_side_move = dir.y;

		// set new button flags.
		if (cmd->m_forward_move > 0.f)
			cmd->m_buttons |= IN_FORWARD;

		else if (cmd->m_forward_move < 0.f)
			cmd->m_buttons |= IN_BACK;

		if (cmd->m_side_move > 0.f)
			cmd->m_buttons |= IN_MOVERIGHT;

		else if (cmd->m_side_move < 0.f)
			cmd->m_buttons |= IN_MOVELEFT;
	}
}

void Movement::AutoPeek() {
	
}

void ClampMovementSpeed(float speed)
{
	// thanks onetap.
	if (!g_cl.m_cmd || !g_cl.m_processing)
		return;

	g_cl.m_cmd->m_buttons |= IN_SPEED;

	float squirt = std::sqrtf((g_cl.m_cmd->m_forward_move * g_cl.m_cmd->m_forward_move) + (g_cl.m_cmd->m_side_move * g_cl.m_cmd->m_side_move));

	if (squirt > speed) {
		float squirt2 = std::sqrtf((g_cl.m_cmd->m_forward_move * g_cl.m_cmd->m_forward_move) + (g_cl.m_cmd->m_side_move * g_cl.m_cmd->m_side_move));

		float cock1 = g_cl.m_cmd->m_forward_move / squirt2;
		float cock2 = g_cl.m_cmd->m_side_move / squirt2;

		if (speed + 1.f <= g_cl.m_local->m_vecVelocity().length_2d()) {
			g_cl.m_cmd->m_forward_move = g_cl.m_cmd->m_side_move = 0.f;
		}
		else {
			g_cl.m_cmd->m_forward_move = cock1 * speed;
			g_cl.m_cmd->m_side_move = cock2 * speed;
		}
	}
}


void Movement::FastStop() {
	if (!g_cl.m_cmd || !g_cl.m_local || !g_cl.m_local->alive())
		return;

	if (!g_cl.m_pressing_move && g_cl.m_speed > 15.f) {
		auto weapon = g_cl.m_local->GetActiveWeapon();

		// don't fake movement while noclipping or on ladders..
		if (!weapon || !weapon->GetWpnData() || g_cl.m_local->m_MoveType() == MOVETYPE_NOCLIP || g_cl.m_local->m_MoveType() == MOVETYPE_LADDER)
			return;

		if (!(g_cl.m_local->m_fFlags() & FL_ONGROUND))
			return;

		if (g_cl.m_cmd->m_buttons & IN_JUMP)
			return;

		auto move_speed = sqrtf((g_cl.m_cmd->m_forward_move * g_cl.m_cmd->m_forward_move) + (g_cl.m_cmd->m_side_move * g_cl.m_cmd->m_side_move));
		auto pre_prediction_velocity = g_cl.m_local->m_vecVelocity().length_2d();

		auto v58 = g_csgo.sv_stopspeed->GetFloat();
		v58 = fmaxf(v58, pre_prediction_velocity);
		v58 = g_csgo.sv_friction->GetFloat() * v58;
		auto slow_walked_speed = fmaxf(pre_prediction_velocity - (v58 * g_csgo.m_globals->m_interval), 0.0f);

		if (slow_walked_speed <= 0 || pre_prediction_velocity <= slow_walked_speed) {
			g_cl.m_cmd->m_forward_move = 0;
			g_cl.m_cmd->m_side_move = 0;
			return;
		}

		ang_t angle;
		math::VectorAngles(g_cl.m_local->m_vecVelocity(), angle);

		// get our current speed of travel.
		float speed = g_cl.m_local->m_vecVelocity().length();

		// fix direction by factoring in where we are looking.
		angle.y = g_cl.m_view_angles.y - angle.y;

		// convert corrected angle back to a direction.
		vec3_t direction;
		math::AngleVectors(angle, &direction);

		vec3_t stop = direction * -speed;

		g_cl.m_cmd->m_forward_move = stop.x;
		g_cl.m_cmd->m_side_move = stop.y;
	}
}

void Movement::NullVelocity(CUserCmd* cmd) {
	vec3_t vel = g_cl.m_local->m_vecVelocity();

	if (vel.length_2d() < 15.f) {
		cmd->m_forward_move = cmd->m_side_move = 0.f;
		return;
	}

	ang_t direction;
	math::VectorAngles(vel, direction);

	ang_t view_angles;
	g_csgo.m_engine->GetViewAngles(view_angles);

	direction.y = view_angles.y - direction.y;

	vec3_t forward;
	math::AngleVectors(direction, &forward);

	static ConVar* cl_forwardspeed = g_csgo.m_cvar->FindVar(HASH("cl_forwardspeed"));
	static ConVar* cl_sidespeed = g_csgo.m_cvar->FindVar(HASH("cl_sidespeed"));

	const vec3_t negative_forward_direction = forward * -cl_forwardspeed->GetFloat();
	const vec3_t negative_side_direction = forward * -cl_sidespeed->GetFloat();

	cmd->m_forward_move = negative_forward_direction.x;
	cmd->m_side_move = negative_side_direction.y;
}

void Movement::AutoStop() {

	if (!(g_cl.m_local->m_fFlags() & FL_ONGROUND))
		return;

	if (g_cl.m_cmd->m_buttons & IN_JUMP)
		return;

	if (!g_cl.m_weapon_info)
		return;

	if (!g_cl.m_weapon) // sanity check
		return;

	if (!g_cl.m_local || !g_cl.m_processing)
		return;

	if (!g_menu.main.aimbot.autostop.get() || !g_aimbot.m_stop)
		return;

	Weapon* wpn = g_cl.m_local->GetActiveWeapon();

	if (!wpn)
		return;

	WeaponInfo* wpn_data = wpn->GetWpnData();


	if (!wpn_data)
		return;

	float max_speed = g_cl.m_weapon->m_zoomLevel() == 0 ? g_cl.m_weapon_info->m_max_player_speed : g_cl.m_weapon_info->m_max_player_speed_alt;

	vec3_t velocity = g_cl.m_local->m_vecVelocity();
	velocity.z = 0.0f;
	float speed = velocity.length_2d();

	// NOTE:: lol pasted from onetap (i think its better than this)
	// NOTE:: if speed is more than 25% max speed
	if (speed > max_speed * 0.2f) {

		ang_t direction;
		ang_t real_view;

		math::VectorAngles(velocity, direction);
		g_csgo.m_engine->GetViewAngles(real_view);

		direction.y = real_view.y - direction.y;

		vec3_t forward;
		math::AngleVectors(direction, &forward);

		static auto cl_forwardspeed = g_csgo.m_cvar->FindVar(HASH("cl_forwardspeed"));
		static auto cl_sidespeed = g_csgo.m_cvar->FindVar(HASH("cl_sidespeed"));

		auto negative_forward_speed = -cl_forwardspeed->GetFloat();
		auto negative_side_speed = -cl_sidespeed->GetFloat();

		auto negative_forward_direction = forward * negative_forward_speed;
		auto negative_side_direction = forward * negative_side_speed;

		g_cl.m_cmd->m_forward_move = negative_forward_direction.x;
		g_cl.m_cmd->m_side_move = negative_side_direction.y;
	}
	else
		// NOTE:: cancer but should do the job i suppose
		ClampMovementSpeed(max_speed * 0.25f);

	return;
}


void Movement::QuickStop() {
	// convert velocity to angular momentum.
	ang_t angle;
	math::VectorAngles(g_cl.m_local->m_vecVelocity(), angle);

	// get our current speed of travel.
	float speed = g_cl.m_local->m_vecVelocity().length();

	// fix direction by factoring in where we are looking.
	angle.y = g_cl.m_view_angles.y - angle.y;

	// convert corrected angle back to a direction.
	vec3_t direction;
	math::AngleVectors(angle, &direction);

	vec3_t stop = direction * -speed;

	if (g_cl.m_speed > 13.f) {
		g_cl.m_cmd->m_forward_move = stop.x;
		g_cl.m_cmd->m_side_move = stop.y;
	}
	else {
		g_cl.m_cmd->m_forward_move = 0.f;
		g_cl.m_cmd->m_side_move = 0.f;
	}
}

void Movement::StopFakeWalkMovement() {
	if (!g_cl.m_cmd || !g_cl.m_local || !g_cl.m_local->alive())
		return;

	auto weapon = g_cl.m_local->GetActiveWeapon();

	// don't fake movement while noclipping or on ladders..
	if (!weapon || !weapon->GetWpnData() || g_cl.m_local->m_MoveType() == MOVETYPE_NOCLIP || g_cl.m_local->m_MoveType() == MOVETYPE_LADDER)
		return;

	if (!(g_cl.m_local->m_fFlags() & FL_ONGROUND))
		return;

	if (g_cl.m_cmd->m_buttons & IN_JUMP)
		return;

	auto move_speed = sqrtf((g_cl.m_cmd->m_forward_move * g_cl.m_cmd->m_forward_move) + (g_cl.m_cmd->m_side_move * g_cl.m_cmd->m_side_move));
	auto pre_prediction_velocity = g_cl.m_local->m_vecVelocity().length_2d();

	auto v58 = g_csgo.sv_stopspeed->GetFloat();
	v58 = fmaxf(v58, pre_prediction_velocity);
	v58 = g_csgo.sv_friction->GetFloat() * v58;
	auto slow_walked_speed = fmaxf(pre_prediction_velocity - (v58 * g_csgo.m_globals->m_interval), 0.0f);

	if (slow_walked_speed <= 0 || pre_prediction_velocity <= slow_walked_speed) {
		g_cl.m_cmd->m_forward_move = 0;
		g_cl.m_cmd->m_side_move = 0;
		return;
	}

	ang_t angle;
	math::VectorAngles(g_cl.m_local->m_vecVelocity(), angle);

	// get our current speed of travel.
	float speed = g_cl.m_local->m_vecVelocity().length();

	// fix direction by factoring in where we are looking.
	angle.y = g_cl.m_view_angles.y - angle.y;

	// convert corrected angle back to a direction.
	vec3_t direction;
	math::AngleVectors(angle, &direction);

	vec3_t stop = direction * -speed;

	g_cl.m_cmd->m_forward_move = stop.x;
	g_cl.m_cmd->m_side_move = stop.y;
}

#define check_if_non_valid_number(x) (fpclassify(x) == FP_INFINITE || fpclassify(x) == FP_NAN || fpclassify(x) == FP_SUBNORMAL)

vec3_t ToVectors(ang_t* ang, vec3_t* side /*= nullptr*/, vec3_t* up /*= nullptr*/) {
	float  rad_pitch = math::deg_to_rad(ang->x);
	float  rad_yaw = math::deg_to_rad(ang->y);
	float sin_pitch;
	float sin_yaw;
	float sin_roll;
	float cos_pitch;
	float cos_yaw;
	float cos_roll;

	math::ScalarSinCos(&sin_pitch, &cos_pitch, rad_pitch);
	math::ScalarSinCos(&sin_yaw, &cos_yaw, rad_yaw);

	if (side || up)
		math::ScalarSinCos(&sin_roll, &cos_roll, math::deg_to_rad(ang->z));

	if (side) {
		side->x = -1.0f * sin_roll * sin_pitch * cos_yaw + -1.0f * cos_roll * -sin_yaw;
		side->y = -1.0f * sin_roll * sin_pitch * sin_yaw + -1.0f * cos_roll * cos_yaw;
		side->z = -1.0f * sin_roll * cos_pitch;
	}

	if (up) {
		up->x = cos_roll * sin_pitch * cos_yaw + -sin_roll * -sin_yaw;
		up->y = cos_roll * sin_pitch * sin_yaw + -sin_roll * cos_yaw;
		up->z = cos_roll * cos_pitch;
	}

	return { cos_pitch * cos_yaw, cos_pitch * sin_yaw, -sin_pitch };
}

ang_t ToEulerAngles(vec3_t ang, vec3_t* pseudoup = nullptr) {
	auto pitch = 0.0f;
	auto yaw = 0.0f;
	auto roll = 0.0f;

	auto length = ang.ToVector2D().length();

	if (pseudoup) {
		auto left = pseudoup->cross(ang);

		left.normalize();

		pitch = math::rad_to_deg(std::atan2(-ang.z, length));

		if (pitch < 0.0f)
			pitch += 360.0f;

		if (length > 0.001f) {
			yaw = math::rad_to_deg(std::atan2(ang.y, ang.x));

			if (yaw < 0.0f)
				yaw += 360.0f;

			auto up_z = (ang.x * left.y) - (ang.y * left.x);

			roll = math::rad_to_deg(std::atan2(left.z, up_z));

			if (roll < 0.0f)
				roll += 360.0f;
		}
		else {
			yaw = math::rad_to_deg(std::atan2(-left.x, left.y));

			if (yaw < 0.0f)
				yaw += 360.0f;
		}
	}
	else {
		if (ang.x == 0.0f && ang.y == 0.0f) {
			if (ang.z > 0.0f)
				pitch = 270.0f;
			else
				pitch = 90.0f;
		}
		else {
			pitch = math::rad_to_deg(std::atan2(-ang.z, length));

			if (pitch < 0.0f)
				pitch += 360.0f;

			yaw = math::rad_to_deg(std::atan2(ang.y, ang.x));

			if (yaw < 0.0f)
				yaw += 360.0f;
		}
	}

	return { pitch, yaw, roll };
}

void RotateMovement(CUserCmd* cmd, ang_t wish_angle, ang_t old_angles) {
	// aimware movement fix, reversed by ph4ge/senator for gucci
	if (old_angles.x != wish_angle.x || old_angles.y != wish_angle.y || old_angles.z != wish_angle.z) {
		vec3_t wish_forward, wish_right, wish_up, cmd_forward, cmd_right, cmd_up;

		auto viewangles = old_angles;
		auto movedata = vec3_t(cmd->m_forward_move, cmd->m_side_move, cmd->m_up_move);
		viewangles.normalize();

		if (viewangles.z != 0.f) {
			auto pLocal = g_cl.m_local;

			if (pLocal && !(pLocal->m_fFlags() & FL_ONGROUND))
				movedata.y = 0.f;
		}

		wish_forward = ToVectors(&wish_angle, &wish_right, &wish_up);
		cmd_forward = ToVectors(&viewangles, &cmd_right, &cmd_up);

		auto v8 = sqrt(wish_forward.x * wish_forward.x + wish_forward.y * wish_forward.y), v10 = sqrt(wish_right.x * wish_right.x + wish_right.y * wish_right.y), v12 = sqrt(wish_up.z * wish_up.z);

		vec3_t wish_forward_norm(1.0f / v8 * wish_forward.x, 1.0f / v8 * wish_forward.y, 0.f),
			wish_right_norm(1.0f / v10 * wish_right.x, 1.0f / v10 * wish_right.y, 0.f),
			wish_up_norm(0.f, 0.f, 1.0f / v12 * wish_up.z);

		auto v14 = sqrt(cmd_forward.x * cmd_forward.x + cmd_forward.y * cmd_forward.y), v16 = sqrt(cmd_right.x * cmd_right.x + cmd_right.y * cmd_right.y), v18 = sqrt(cmd_up.z * cmd_up.z);

		vec3_t cmd_forward_norm(1.0f / v14 * cmd_forward.x, 1.0f / v14 * cmd_forward.y, 1.0f / v14 * 0.0f),
			cmd_right_norm(1.0f / v16 * cmd_right.x, 1.0f / v16 * cmd_right.y, 1.0f / v16 * 0.0f),
			cmd_up_norm(0.f, 0.f, 1.0f / v18 * cmd_up.z);

		auto v22 = wish_forward_norm.x * movedata.x, v26 = wish_forward_norm.y * movedata.x, v28 = wish_forward_norm.z * movedata.x, v24 = wish_right_norm.x * movedata.y, v23 = wish_right_norm.y * movedata.y, v25 = wish_right_norm.z * movedata.y, v30 = wish_up_norm.x * movedata.z, v27 = wish_up_norm.z * movedata.z, v29 = wish_up_norm.y * movedata.z;

		cmd->m_forward_move = cmd_forward_norm.x * v24 + cmd_forward_norm.y * v23 + cmd_forward_norm.z * v25 + (cmd_forward_norm.x * v22 + cmd_forward_norm.y * v26 + cmd_forward_norm.z * v28) + (cmd_forward_norm.y * v30 + cmd_forward_norm.x * v29 + cmd_forward_norm.z * v27);
		cmd->m_side_move = cmd_right_norm.x * v24 + cmd_right_norm.y * v23 + cmd_right_norm.z * v25 + (cmd_right_norm.x * v22 + cmd_right_norm.y * v26 + cmd_right_norm.z * v28) + (cmd_right_norm.x * v29 + cmd_right_norm.y * v30 + cmd_right_norm.z * v27);
		cmd->m_up_move = cmd_up_norm.x * v23 + cmd_up_norm.y * v24 + cmd_up_norm.z * v25 + (cmd_up_norm.x * v26 + cmd_up_norm.y * v22 + cmd_up_norm.z * v28) + (cmd_up_norm.x * v30 + cmd_up_norm.y * v29 + cmd_up_norm.z * v27);

		cmd->m_forward_move = std::clamp(cmd->m_forward_move, -g_csgo.m_cvar->FindVar(HASH("cl_forwardspeed"))->GetFloat(), g_csgo.m_cvar->FindVar(HASH("cl_forwardspeed"))->GetFloat());
		cmd->m_side_move = std::clamp(cmd->m_side_move, -g_csgo.m_cvar->FindVar(HASH("cl_sidespeed"))->GetFloat(), g_csgo.m_cvar->FindVar(HASH("cl_sidespeed"))->GetFloat());
		cmd->m_up_move = std::clamp(cmd->m_up_move, -g_csgo.m_cvar->FindVar(HASH("cl_upspeed"))->GetFloat(), g_csgo.m_cvar->FindVar(HASH("cl_upspeed"))->GetFloat());
	}
}

void InstantStop(CUserCmd* cmd) {
	float maxSpeed = 260.f;

	if (const auto weapon = g_cl.m_local->GetActiveWeapon(); weapon != nullptr)
		if (const auto data = weapon->GetWpnData(); data != nullptr)
			maxSpeed = g_cl.m_local->m_bIsScoped() ? data->m_max_player_speed_alt : data->m_max_player_speed;

	vec3_t velocity = g_cl.m_local->m_vecVelocity();
	velocity.z = 0.0f;

	CUserCmd* ucmd = cmd ? cmd : g_cl.m_cmd;

	float speed = velocity.length_2d();

	float playerSurfaceFriction = g_cl.m_local->m_surfaceFriction();
	float max_accelspeed = g_csgo.m_cvar->FindVar(HASH("sv_accelerate"))->GetFloat() * g_csgo.m_globals->m_interval * maxSpeed * playerSurfaceFriction;
	if (speed - max_accelspeed <= -1.f) {
		ucmd->m_forward_move = speed / max_accelspeed;
	}
	else {
		ucmd->m_forward_move = g_csgo.m_cvar->FindVar(HASH("cl_forwardspeed"))->GetFloat();
	}

	ucmd->m_side_move = 0.0f;

	ang_t move_dir = g_cl.m_view_angles;

	float direction = atan2(velocity.y, velocity.x);
	move_dir.y = std::remainderf(math::rad_to_deg(direction) + 180.0f, 360.0f);
	RotateMovement(ucmd, move_dir, ucmd->m_view_angles);
}

void Movement::FakeWalk() {
	if (!g_cl.m_local || !g_cl.m_processing)
		return;

	if (!g_input.GetKeyState(g_menu.main.misc.fakewalk.get()))
		return;

	int choked = g_menu.main.antiaim.lag_limit.get();
	float speed_ticks = 15;

	if (choked < speed_ticks)
		speed_ticks = choked;

	// choke
	*g_cl.m_packet = g_csgo.m_cl->m_choked_commands > speed_ticks;

	// set as slowwalk
	g_cl.m_cmd->m_buttons &= ~IN_SPEED;

	// compensate for lost speed
	int nTicksToStop = speed_ticks * 35 / 100; // 35% of the choke limit
	if (g_cl.m_local->m_bIsScoped() && g_cl.m_weapon_id != SSG08)
		nTicksToStop = 2;

	// stop when necessary
	if (g_csgo.m_cl->m_choked_commands >= (speed_ticks - nTicksToStop) || !g_csgo.m_cl->m_choked_commands) {
		InstantStop(g_cl.m_cmd);
	}
}