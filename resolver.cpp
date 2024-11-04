#include "includes.h"
Resolver g_resolver{};;

void Resolver::miss_reset(int index) {
	AimPlayer* data = &g_aimbot.m_players[index - 1];

	data->m_move_index = 0;
	for (int i = 0; i < 7; i++)
	data->m_stand_int[i] = 0;
	data->m_air_index = 0;
	data->m_body_index = 0;
}

float Resolver::AutoDirection(LagRecord* record, float multiplier) {
	// constants
	constexpr float STEP{ 4.f };
	constexpr float RANGE{ 32.f };

	// best target.
	vec3_t enemypos;
	record->m_player->GetEyePos(&enemypos);
	float away = GetAwayAngle(record->m_player);

	// construct vector of angles to test.
	std::vector< AdaptiveAngle1 > angles{ };
	angles.emplace_back(away, 180.f);
	angles.emplace_back(away, 90.f);
	angles.emplace_back(away, -90.f);

	// start the trace at the enemy shoot pos.
	vec3_t start;
	g_cl.m_local->GetEyePos(&start);

	// see if we got any valid result.
	// if this is false the path was not obstructed with anything.
	bool valid{ false };

	// iterate vector of angles.
	for (auto it = angles.begin(); it != angles.end(); ++it) {
		float yaw = math::NormalizedAngle(it->m_yaw1 + it->m_add1);

		// compute the 'rough' estimation of where our head will be.
		vec3_t end{ enemypos.x + std::cos(math::deg_to_rad(yaw)) * RANGE,
			enemypos.y + std::sin(math::deg_to_rad(yaw)) * RANGE,
			enemypos.z };

		// compute the direction.
		vec3_t dir = start - end;
		float len = dir.normalize();

		// should never happen.
		if (len <= 0.f)
			continue;

		// step thru the total distance, 4 units per step.
		for (float i{ 0.f }; i < len; i += STEP) {
			// get the current step position.
			vec3_t point = end + (dir * i);

			// get the contents at this point.
			int contents = g_csgo.m_engine_trace->GetPointContents(point, MASK_SHOT_HULL);

			// contains nothing that can stop a bullet.
			if (!(contents & MASK_SHOT_HULL))
				continue;

			float mult = 1.f;

			// over 50% of the total length, prioritize this shit.
			if (i > (len * 0.5f))
				mult = 1.25f;

			// over 90% of the total length, prioritize this shit.
			if (i > (len * 0.75f))
				mult = 1.25f;

			// over 90% of the total length, prioritize this shit.
			if (i > (len * 0.9f))
				mult = 2.f;

			// append 'penetrated distance'.
			it->m_dist1 += (STEP * mult);

			// mark that we found anything.
			valid = true;
		}
	}

	if (!valid)
		return math::NormalizedAngle(away - 180.f);

	// put the most distance at the front of the container.
	std::sort(angles.begin(), angles.end(),
		[](const AdaptiveAngle1& a, const AdaptiveAngle1& b) {
			return a.m_dist1 > b.m_dist1;
		});

	// the best angle should be at the front now.
	AdaptiveAngle1* best = &angles.front();

	return math::NormalizedAngle(best->m_yaw1 + (best->m_add1 * multiplier));
}

void Resolver::FindBestAngle(Player* player, LagRecord* record) {
	AimPlayer* data = &g_aimbot.m_players[player->index() - 1];

	// constants.
	constexpr float STEP{ 4.f };
	constexpr float RANGE{ 28.f };

	// get the away angle for this record.
	float away = GetAwayAngle(record->m_player);

	vec3_t enemy_eyepos = player->wpn_shoot_pos();

	// construct vector of angles to test.
	std::vector< AdaptiveAngle > angles{ };
	angles.emplace_back(away + 90.f);
	angles.emplace_back(away - 90.f);

	// start the trace at the enemy shoot pos.
	vec3_t start = g_cl.m_local->wpn_shoot_pos();

	// see if we got any valid result.
	// if this is false the path was not obstructed with anything.
	bool valid{ false };

	// iterate vector of angles.
	for (auto it = angles.begin(); it != angles.end(); ++it) {

		// compute the 'rough' estimation of where our head will be.
		vec3_t end{ enemy_eyepos.x + std::cos(math::deg_to_rad(it->m_yaw)) * RANGE,
			enemy_eyepos.y + std::sin(math::deg_to_rad(it->m_yaw)) * RANGE,
			enemy_eyepos.z };

		// draw a line for debugging purposes.
		//g_csgo.m_debug_overlay->AddLineOverlay( start, end, 255, 0, 0, true, 0.1f );

		// compute the direction.
		vec3_t dir = end - start;
		float len = dir.normalize();

		// should never happen.
		if (len <= 0.f)
			continue;

		// step thru the total distance, 4 units per step.
		for (float i{ 0.f }; i < len; i += STEP) {
			// get the current step position.
			vec3_t point = start + (dir * i);

			// get the contents at this point.
			int contents = g_csgo.m_engine_trace->GetPointContents(point, MASK_SHOT_HULL);

			// contains nothing that can stop a bullet.
			if (!(contents & MASK_SHOT_HULL))
				continue;

			float mult = 1.f;

			// over 50% of the total length, prioritize this shit.
			if (i > (len * 0.5f))
				mult = 1.25f;

			// over 90% of the total length, prioritize this shit.
			if (i > (len * 0.75f))
				mult = 1.5f;

			// over 90% of the total length, prioritize this shit.
			if (i > (len * 0.9f))
				mult = 2.f;

			// append 'penetrated distance'.
			it->m_dist += (STEP * mult);

			// mark that we found anything.
			valid = true;
		}
	}

	if (!valid) {
		data->freestand_data = false;
		data->m_anti_fs_angle = away + 180.f;
		return;
	}

	// put the most distance at the front of the container.
	std::sort(angles.begin(), angles.end(),
		[](const AdaptiveAngle& a, const AdaptiveAngle& b) {
		return a.m_dist > b.m_dist;
	});

	// the best angle should be at the front now.
	AdaptiveAngle* best = &angles.front();

	data->m_anti_fs_angle = math::NormalizedAngle(best->m_yaw);
	data->freestand_data = true;
}

LagRecord* Resolver::FindIdealRecord(AimPlayer* data) {
	LagRecord* first_valid, *current;

	if (data->m_records.empty())
		return nullptr;

	first_valid = nullptr;

	// iterate records.
	for (const auto& it : data->m_records) {
		if (it->dormant() || it->immune() || !it->valid() || it->brokelc() || !it->m_setup)
			continue;

		// get current record.
		current = it.get();

		// first record that was valid, store it for later.
		if (!first_valid)
			first_valid = current;

		// try to find a record with a shot, lby update, walking or no anti-aim.
		if (it->m_shot || it->m_mode == Modes::RESOLVE_BODY || it->m_mode == Modes::RESOLVE_WALK || it->m_mode == Modes::RESOLVE_NONE)
			return current;
	}

	// none found above, return the first valid record if possible.
	return (first_valid) ? first_valid : nullptr;
}

LagRecord* Resolver::FindLastRecord(AimPlayer* data) {
	LagRecord* current;

	if (data->m_records.empty())
		return nullptr;

	// iterate records in reverse.
	for (auto it = data->m_records.crbegin(); it != data->m_records.crend(); ++it) {
		current = it->get();

		// if this record is valid.
		// we are done since we iterated in reverse.
		if (current->valid() && !current->immune() && !current->dormant() && !current->brokelc() && current->m_setup)
			return current;
	}

	return nullptr;
}

void Resolver::OnBodyUpdate(Player* player, float value) {
	AimPlayer* data = &g_aimbot.m_players[player->index() - 1];

	// set data.
	data->m_old_body = data->m_body;
	data->m_body = value;

	if (player->m_vecVelocity().length_2d() > 0.1f || !(player->m_fFlags() & FL_ONGROUND)) {
		data->m_body_proxy_updated = false;
		data->m_body_proxy_old = value;
		data->m_body_proxy = value;
		return;
	}

	// lol
	if (fabsf(math::angle_diff(value, data->m_body_proxy)) >= 35.f) {
		data->m_body_proxy_old = data->m_body_proxy;
		data->m_body_proxy = value;
		data->m_body_proxy_updated = true;
	}
}

float Resolver::GetAwayAngle(Player* player) {
	ang_t away;
	math::VectorAngles(g_cl.m_local->m_vecOrigin() - player->m_vecOrigin(), away);
	return away.y;
}

void Resolver::MatchShot(AimPlayer* data, LagRecord* record) {
	// do not attempt to do this in nospread mode.
	if (g_menu.main.config.mode.get() == 1)
		return;

	Weapon* wpn = data->m_player->GetActiveWeapon();

	if (!wpn)
		return;

	WeaponInfo* wpn_data = wpn->GetWpnData();

	if (!wpn_data)
		return;

	if ((wpn_data->m_weapon_type != WEAPONTYPE_GRENADE && wpn_data->m_weapon_type > 6) || wpn_data->m_weapon_type <= 0)
		return;

	const auto shot_time = wpn->m_fLastShotTime();
	const auto shot_tick = game::TIME_TO_TICKS(shot_time);

	if (shot_tick == game::TIME_TO_TICKS(record->m_sim_time) && record->m_lag <= 2)
		record->m_shot_type = 2;
	else {
		bool should_correct_pitch = false;

		if (shot_tick == game::TIME_TO_TICKS(record->m_anim_time)) {
			record->m_shot_type = 1;
			should_correct_pitch = true;
		}
		else if (shot_tick >= game::TIME_TO_TICKS(record->m_anim_time)) {
			if (shot_tick <= game::TIME_TO_TICKS(record->m_sim_time))
				should_correct_pitch = true;
		}

		if (should_correct_pitch) {
			float valid_pitch = 89.f;

			for (const auto& it : data->m_records) {
				if (it.get() == record || it->dormant() || it->immune())
					continue;

				if (it->m_shot_type <= 0) {
					valid_pitch = it->m_eye_angles.x;
					break;
				}
			}

			record->m_eye_angles.x = valid_pitch;
		}
	}
}

void Resolver::SetMode(LagRecord* record) {
	// the resolver has 3 modes to chose from.
	// these modes will vary more under the hood depending on what data we have about the player
	// and what kind of hack vs. hack we are playing (mm/nospread).

	float speed = record->m_anim_velocity.length();

	if (g_input.GetKeyState(g_menu.main.aimbot.override.get()) && record->m_flags & FL_ONGROUND && (speed <= 0.1f || record->m_fake_walk))
		record->m_mode = Modes::RESOLVE_OVERRIDE;
	else if ((record->m_flags & FL_ONGROUND) && speed > 0.1f && !record->m_fake_walk)
		record->m_mode = Modes::RESOLVE_WALK;
	else if ((record->m_flags & FL_ONGROUND) && (speed <= 0.1f || record->m_fake_walk))
		record->m_mode = Modes::RESOLVE_STAND;
	else if (!(record->m_flags & FL_ONGROUND))
		record->m_mode = Modes::RESOLVE_AIR;
}

void Resolver::ResolveAngles(Player* player, LagRecord* record, LagRecord* previous) {
	AimPlayer* data = &g_aimbot.m_players[player->index() - 1];

	CCSGOPlayerAnimState* state = player->m_PlayerAnimState();
	if (!state)
		return;

	// mark this record if it contains a shot.
	MatchShot(data, record);

	// next up mark this record with a resolver mode that will be used.
	SetMode(record);

	// wait we might actually need this
	FindBestAngle(player, record);

	switch (record->m_mode) {
	case Modes::RESOLVE_WALK:
		ResolveWalk(data, record, player);
		break;
	case Modes::RESOLVE_STAND:
		ResolveStand(data, record, state);
		break;
	case Modes::RESOLVE_AIR:
		ResolveAir(data, record, previous, state);
		break;
	case Modes::RESOLVE_OVERRIDE:
		ResolveOverride(player, record, data, previous);
		break;
	}

	// normalize the eye angles, doesn't really matter but its clean.
	math::NormalizeAngle(record->m_eye_angles.y);
}

void Resolver::ResolveWalk(AimPlayer* data, LagRecord* record, Player* player) {
	// apply lby to eyeangles.
	record->m_eye_angles.y = record->m_body;

	// delay body update.
	data->m_body_update = record->m_anim_time + 0.22f;

	// reset stand and body index.
	if (record->m_velocity.length_2d() > 45.f) {
		data->m_test_index = 0;
		data->m_fakewalk_index = 0;
		data->m_testfs_index = 0;
		data->m_logic_index = 0;
		data->m_airback_index = 0;
		data->m_stand_index4 = 0;
		data->m_air_index = 0;
		data->m_lowlby_index = 0;
		data->m_airlby_index = 0;
		data->m_spin_index = 0;
		data->m_stand_index1 = 0;
		data->m_stand_index2 = 0;
		data->m_stand_index3 = 0;
		data->m_edge_index = 0;
		data->m_fam_reverse_index = 0;
		data->m_back_index = 0;
		data->m_reversefs_index = 0;
		data->m_back_at_index = 0;
		data->m_reversefs_at_index = 0;
		data->m_lastmove_index = 0;
		data->m_lby_index = 0;
		data->m_airlast_index = 0;
		data->m_body_index = 0;
		data->m_lbyticks = 0;
		data->m_sidelast_index = 0;
		data->m_moving_index = 0;

		// reset data about moving and lby
		// @ruka: lol this was never set to true anywhere
		// @ruka: remove that if it fucks up resolver somehow
		data->is_last_moving_lby_valid = true;
	}

	data->m_broke_lby = false;
	data->m_has_body_updated = false;

	// reset these too
	data->m_stored_body_old = record->m_body;
	data->m_stored_body = record->m_body;
	data->m_body_proxy_updated = false;
	data->m_body_proxy_old = record->m_body;
	data->m_body_proxy = record->m_body;

	// copy the last record that this player was walking
	// we need it later on because it gives us crucial data.
	std::memcpy(&data->m_walk_record, record, sizeof(LagRecord));
}

bool Resolver::IsYawSideways(Player* entity, float yaw)
{
	auto local_player = g_cl.m_local;
	if (!local_player)
		return false;

	const auto at_target_yaw = math::CalcAngle(local_player->m_vecOrigin(), entity->m_vecOrigin()).y;
	const float delta = fabs(math::AngleDiff(at_target_yaw, yaw));

	return delta > 35.f && delta < 145.f;
}

void Resolver::ResolveStand(AimPlayer* data, LagRecord* record, CCSGOPlayerAnimState* state) {
	const float away = math::NormalizedAngle(GetAwayAngle(record->m_player) + 180.f);
	const int act = data->m_player->GetSequenceActivity(record->m_layers[3].m_sequence);

	if (data->m_walk_stuff.m_sim_tick == 0) {
		switch (data->m_stand_int[StandRecords::STAND] % 3) {
		case 0:
			record->m_eye_angles.y = record->m_body;
			break;
		case 1:
			record->m_eye_angles.y = away;
			break;
		case 2:
			record->m_eye_angles.y = data->m_freestand[0];
			break;
		}
		return;
	}

	if (data->m_moved) {
		float body_delta = data->m_body_delta;

		if (fabs(body_delta) <= 120.f && act != 980) {
			if (record->m_layers[3].m_weight <= 0.f)
				body_delta = -fabs(body_delta);
			else
				body_delta = fabs(body_delta);
		}

		const float deltas[] = { math::NormalizedAngle(record->m_body - away),
			math::NormalizedAngle(record->m_body - data->m_walk_stuff.m_body),
		};

		if (act != 979)
			body_delta = std::max(std::min(body_delta, 120.f), -35.f);
		else {
			if (fabs(body_delta) <= 35.f && fabs(deltas[1]) <= 35.f && fabs(deltas[0]) > 60.f && fabs(deltas[0]) < 120.f) {
				body_delta = 180.f;
			}

			//body_delta = std::min(std::max(body_delta, 120.f), -35.f);
		}

		const bool preflick = fabs(body_delta) <= 35.0f && fabs(body_delta) > 0.0f && act == 979;
		const bool sideways = fabs(deltas[0]) > 45.f && fabs(deltas[0]) < 135.f;

		if (data->m_stand_int[StandRecords::DELTA] <= 2) {
			record->m_mode = Modes::RESOLVE_STAND_DELTA;
			switch (data->m_stand_int[StandRecords::DELTA] % 3) {
			case 0:
				record->m_eye_angles.y = record->m_body - body_delta;
				break;
			case 1:
				record->m_eye_angles.y = data->m_walk_stuff.m_body;
				break;
			case 2:
				record->m_eye_angles.y = record->m_body + body_delta;
				break;
			}
			return;
		}
	}

	switch (data->m_stand_int[StandRecords::STAND] % 3) {
	case 0:
		record->m_eye_angles.y = data->m_freestand[0];
		break;
	case 1:
		record->m_eye_angles.y = away;
		break;
	case 2:
		record->m_eye_angles.y = data->m_freestand[1];
		break;
	}
}

void Resolver::StandNS(AimPlayer* data, LagRecord* record) {
	// get away angles.
	float away = GetAwayAngle(record->m_player);

	switch (data->m_shots % 8) {
	case 0:
		record->m_eye_angles.y = away + 180.f;
		break;

	case 1:
		record->m_eye_angles.y = away + 90.f;
		break;
	case 2:
		record->m_eye_angles.y = away - 90.f;
		break;

	case 3:
		record->m_eye_angles.y = away + 45.f;
		break;
	case 4:
		record->m_eye_angles.y = away - 45.f;
		break;

	case 5:
		record->m_eye_angles.y = away + 135.f;
		break;
	case 6:
		record->m_eye_angles.y = away - 135.f;
		break;

	case 7:
		record->m_eye_angles.y = away + 0.f;
		break;

	default:
		break;
	}

	// force LBY to not fuck any pose and do a true bruteforce.
	record->m_body = record->m_eye_angles.y;
}

void Resolver::ResolveAir(AimPlayer* data, LagRecord* record, LagRecord* previous, CCSGOPlayerAnimState* state) {
	// for no-spread call a seperate resolver.
	if (g_menu.main.config.mode.get() == 1) {
		AirNS(data, record);
		return;
	}
	float away = math::NormalizedAngle(GetAwayAngle(record->m_player) + 180.f);
	ang_t velocity_angle = { 0, away, 0 };

	if (record->m_velocity.length_2d() >= 0.001f)
		math::VectorAngles(record->m_velocity, velocity_angle);

	switch (data->m_air_index % 3) {
	case 0:
		record->m_eye_angles.y = record->m_body;
		break;
	case 1:
		record->m_eye_angles.y = velocity_angle.y + 180.f;
		break;
	case 2:
		record->m_eye_angles.y = away;
		break;
	}

	/* force abs yaw for safety */
	state->m_abs_yaw = math::NormalizedAngle(record->m_eye_angles.y);
}

void Resolver::AirNS(AimPlayer* data, LagRecord* record) {
	// get away angles.
	float away = GetAwayAngle(record->m_player);

	switch (data->m_shots % 9) {
	case 0:
		record->m_eye_angles.y = away + 180.f;
		break;

	case 1:
		record->m_eye_angles.y = away + 150.f;
		break;
	case 2:
		record->m_eye_angles.y = away - 150.f;
		break;

	case 3:
		record->m_eye_angles.y = away + 165.f;
		break;
	case 4:
		record->m_eye_angles.y = away - 165.f;
		break;

	case 5:
		record->m_eye_angles.y = away + 135.f;
		break;
	case 6:
		record->m_eye_angles.y = away - 135.f;
		break;

	case 7:
		record->m_eye_angles.y = away + 90.f;
		break;
	case 8:
		record->m_eye_angles.y = away - 90.f;
		break;

	default:
		break;
	}
}

void Resolver::ResolvePoses(Player* player, LagRecord* record) {
	AimPlayer* data = &g_aimbot.m_players[player->index() - 1];

	// only do this bs when in air.
	if (record->m_mode == Modes::RESOLVE_AIR) {
		// ang = pose min + pose val x ( pose range )

		// lean_yaw
		player->m_flPoseParameter()[2] = g_csgo.RandomInt(0, 4) * 0.25f;

		// body_yaw
		player->m_flPoseParameter()[11] = g_csgo.RandomInt(1, 3) * 0.25f;
	}
}


void Resolver::ResolveOverride(Player* player, LagRecord* record, AimPlayer* data, LagRecord* previous) {

	// get predicted away angle for the player.
	float away = GetAwayAngle(record->m_player);

	CCSGOPlayerAnimState* state = player->m_PlayerAnimState();
	if (!state)
		return;

	// pointer for easy access.
	LagRecord* move = &data->m_walk_record;

	C_AnimationLayer* curr = &record->m_layers[3];
	int act = data->m_player->GetSequenceActivity(curr->m_sequence);

	if (g_input.GetKeyState(g_menu.main.aimbot.override.get())) {
		ang_t                          viewangles;
		g_csgo.m_engine->GetViewAngles(viewangles);

		//auto yaw = math::clamp (g_cl.m_local->GetAbsOrigin(), Player->origin()).y;
		const float at_target_yaw = math::CalcAngle(g_cl.m_local->m_vecOrigin(), player->m_vecOrigin()).y;

		if (fabs(math::NormalizedAngle(viewangles.y - at_target_yaw)) > 30.f)
			return ResolveStand(data, record, state);

		record->m_eye_angles.y = (math::NormalizedAngle(viewangles.y - at_target_yaw) > 0) ? at_target_yaw + 90.f : at_target_yaw - 90.f;

		//return UTILS::GetLBYRotatedYaw(entity->m_flLowerBodyYawTarget(), (math::NormalizedAngle(viewangles.y - at_target_yaw) > 0) ? at_target_yaw + 90.f : at_target_yaw - 90.f);

		record->m_mode = Modes::RESOLVE_OVERRIDE;
	}

	bool did_lby_flick{ false };

	if (data->m_body != data->m_old_body)
	{
		record->m_eye_angles.y = record->m_body;

		data->m_body_update = record->m_anim_time + 1.1f;

		iPlayers[record->m_player->index()] = false;
		record->m_mode = Modes::RESOLVE_BODY;
	}
	else
	{
		// LBY SHOULD HAVE UPDATED HERE.
		if (record->m_anim_time >= data->m_body_update) {
			// only shoot the LBY flick 3 times.
			// if we happen to miss then we most likely mispredicted
			if (data->m_body_index < 1) {
				// set angles to current LBY.
				record->m_eye_angles.y = record->m_body;

				data->m_body_update = record->m_anim_time + 1.1f;

				// set the resolve mode.
				iPlayers[record->m_player->index()] = false;
				record->m_mode = Modes::RESOLVE_BODY;
			}
		}
	}
}