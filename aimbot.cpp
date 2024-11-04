#include "includes.h"

Aimbot g_aimbot{ };;
/*
void Aimbot::update_shoot_pos() {
	const auto anim_state = g_cl.m_local->m_PlayerAnimState();
	if (!anim_state)
		return;

	const auto backup = g_cl.m_local->m_flPoseParameter()[12];
	const auto absorigin = g_cl.m_local->GetAbsOrigin();

	// set pitch, rotation etc
	g_cl.m_local->m_flPoseParameter()[12] = 0.5f;
	g_cl.m_local->SetAbsAngles(g_cl.m_rotation);
	g_cl.m_local->SetAbsOrigin(g_cl.m_local->m_vecOrigin());

	// setup bones
	if (g_cl.m_local->SetupBones(g_cl.m_local_bones, 128, BONE_USED_BY_ANYTHING, game::TICKS_TO_TIME(g_cl.m_local->m_nTickBase())))
		g_cl.m_shoot_pos = g_cl.m_local->GetShootPosition(g_cl.m_local_bones); // get corrected shootpos

	// set to backup
	g_cl.m_local->SetAbsOrigin(absorigin);
	g_cl.m_local->m_flPoseParameter()[12] = backup;
}
*/

void FixVelocity(Player* m_player, LagRecord* record, LagRecord* previous, float max_speed) {
	if (!previous) {
		if (record->m_layers[6].m_playback_rate > 0.0f && record->m_layers[6].m_weight != 0.f && record->m_velocity.length() > 0.1f) {
			auto v30 = max_speed;

			if (record->m_flags & 6)
				v30 *= 0.34f;
			else if (m_player->m_bIsWalking())
				v30 *= 0.52f;

			auto v35 = record->m_layers[6].m_weight * v30;
			record->m_velocity *= v35 / record->m_velocity.length();
		}
		else
			record->m_velocity.clear();

		if (record->m_flags & 1)
			record->m_velocity.z = 0.f;

		record->m_anim_velocity = record->m_velocity;
		return;
	}

	if ((m_player->m_fEffects() & 8) != 0
		|| m_player->m_ubEFNoInterpParity() != m_player->m_ubEFNoInterpParityOld()) {
		record->m_velocity.clear();
		record->m_anim_velocity.clear();
		return;
	}

	auto is_jumping = !(record->m_flags & FL_ONGROUND && previous->m_flags & FL_ONGROUND);

	if (record->m_lag >= 2) {
		record->m_velocity.clear();
		auto origin_delta = (record->m_origin - previous->m_origin);

		if (!(previous->m_flags & FL_ONGROUND || record->m_flags & FL_ONGROUND))// if not previous on ground or on ground
		{
			auto currently_ducking = record->m_flags & FL_DUCKING;
			if ((previous->m_flags & FL_DUCKING) != currently_ducking) {
				float duck_modifier = 0.f;

				if (currently_ducking)
					duck_modifier = 9.f;
				else
					duck_modifier = -9.f;

				origin_delta.z -= duck_modifier;
			}
		}

		auto sqrt_delta = origin_delta.length_2d_sqr();

		if (sqrt_delta > 0.f && sqrt_delta < 1000000.f)
			record->m_velocity = origin_delta / game::TICKS_TO_TIME(record->m_lag);

		record->m_velocity.validate_vec();

		if (is_jumping) {
			if (record->m_flags & FL_ONGROUND && !g_csgo.sv_enablebunnyhopping->GetInt()) {

				// 260 x 1.1 = 286 units/s.
				float max = m_player->m_flMaxspeed() * 1.1f;

				// get current velocity.
				float speed = record->m_velocity.length();

				// reset velocity to 286 units/s.
				if (max > 0.f && speed > max)
					record->m_velocity *= (max / speed);
			}

			// assume the player is bunnyhopping here so set the upwards impulse.
			record->m_velocity.z = g_csgo.sv_jump_impulse->GetFloat();
		}
		// we are not on the ground
		// apply gravity and airaccel.
		else if (!(record->m_flags & FL_ONGROUND)) {
			// apply one tick of gravity.
			record->m_velocity.z -= g_csgo.sv_gravity->GetFloat() * g_csgo.m_globals->m_interval;
		}
	}


	if (record->m_flags & FL_ONGROUND
		&& record->m_layers[6].m_weight == 0.f
		&& record->m_lag > 2
		&& record->m_velocity.length() > 0.f
		&& record->m_velocity.length() <= record->max_speed * 0.34f)
		record->m_fake_walk = true;

	if (record->m_flags & FL_ONGROUND
		&& record->m_layers[6].m_weight == 0.f
		&& record->m_lag > 6)
		record->m_fake_walk = true;

	record->m_anim_velocity = record->m_velocity;

	if (!record->m_fake_walk) {
		if (record->m_anim_velocity.length_2d() > 0 && (record->m_flags & FL_ONGROUND)) {
			float anim_speed = 0.f;

			if (!is_jumping
				&& record->m_layers[11].m_weight > 0.0f
				&& record->m_layers[11].m_weight < 1.0f
				&& record->m_layers[11].m_playback_rate == previous->m_layers[11].m_playback_rate) {
				// calculate animation speed yielded by anim overlays
				auto flAnimModifier = 0.35f * (1.0f - record->m_layers[11].m_weight);
				if (flAnimModifier > 0.0f && flAnimModifier < 1.0f)
					anim_speed = max_speed * (flAnimModifier + 0.55f);
			}

			// this velocity is valid ONLY IN ANIMFIX UPDATE TICK!!!
			// so don't store it to record as m_vecVelocity
			// -L3D451R7
			if (anim_speed > 0.0f) {
				anim_speed /= record->m_anim_velocity.length_2d();
				record->m_anim_velocity.x *= anim_speed;
				record->m_anim_velocity.y *= anim_speed;
			}
		}
	}
	else
	{
		record->m_velocity.Init();
		record->m_anim_velocity.Init();
	}

	record->m_anim_velocity.validate_vec();
}

void AimPlayer::UpdateAnimations(LagRecord* record) {
	CCSGOPlayerAnimState* state = this->m_player->m_PlayerAnimState();
	if (!state)
		return;

	// player respawned.
	if (this->m_player->m_flSpawnTime() != this->m_spawn) {
		// reset animation state.
		game::ResetAnimationState(state);

		// note new spawn time.
		this->m_spawn = this->m_player->m_flSpawnTime();
	}

	// backup curtime.
	const float m_flRealtime = g_csgo.m_globals->m_realtime;
	const float m_flCurtime = g_csgo.m_globals->m_curtime;
	const float m_flFrametime = g_csgo.m_globals->m_frametime;
	const float m_flAbsFrametime = g_csgo.m_globals->m_abs_frametime;
	const int   m_iFramecount = g_csgo.m_globals->m_frame;
	const int   m_iTickcount = g_csgo.m_globals->m_tick_count;
	const float interpolation = g_csgo.m_globals->m_interp_amt;
	float time = record->m_lag < 2 ? record->m_sim_time : record->m_anim_time;

	// set curtime to animtime.
	// set frametime to ipt just like on the server during simulation.
	g_csgo.m_globals->m_realtime = time;
	g_csgo.m_globals->m_curtime = time;
	g_csgo.m_globals->m_frametime = g_csgo.m_globals->m_interval;
	g_csgo.m_globals->m_abs_frametime = g_csgo.m_globals->m_interval;
	g_csgo.m_globals->m_frame = game::TIME_TO_TICKS(time);
	g_csgo.m_globals->m_tick_count = game::TIME_TO_TICKS(time);
	g_csgo.m_globals->m_interp_amt = 0.f;

	// backup stuff that we do not want to fuck with.
	AnimationBackup_t backup;

	backup.m_origin = this->m_player->m_vecOrigin();
	backup.m_abs_origin = this->m_player->GetAbsOrigin();
	backup.m_velocity = this->m_player->m_vecVelocity();
	backup.m_abs_velocity = this->m_player->m_vecAbsVelocity();
	backup.m_flags = this->m_player->m_fFlags();
	backup.m_eflags = this->m_player->m_iEFlags();
	backup.m_duck = this->m_player->m_flDuckAmount();
	backup.m_body = this->m_player->m_flLowerBodyYawTarget();
	this->m_player->GetAnimLayers(backup.m_layers);

	LagRecord* previous = nullptr;
	LagRecord* pre_previous = nullptr;

	if (m_records.size() >= 2)
		previous = this->m_records[1].get();

	if (m_records.size() >= 3)
		pre_previous = this->m_records[2].get();

	// is player a bot?
	bool bot = game::IsFakePlayer(this->m_player->index());

	// reset fakewalk state.
	record->m_fake_walk = false;
	record->m_mode = Resolver::Modes::RESOLVE_NONE;
	record->m_weapon = this->m_player->GetActiveWeapon();

	// do we have a valid weapon?
	if (record->m_weapon) {

		// get data of that weapon
		WeaponInfo* wpn_data = record->m_weapon->GetWpnData();

		// get weapon max speed
		if (wpn_data)
			record->max_speed = this->m_player->m_bIsScoped() ? wpn_data->m_max_player_speed_alt : wpn_data->m_max_player_speed;
	}

	// @evitable NOTE : check for lag because we dont wanna fix velocity on players shifting tickabse
	if (record->m_lag > 1)
		FixVelocity(this->m_player, record, previous, record->max_speed);
	else
		record->m_velocity = record->m_anim_velocity = m_player->m_vecVelocity();

	// get network lag
	record->m_sim_ticks = game::TIME_TO_TICKS(this->m_player->m_flSimulationTime() - this->m_player->m_flOldSimulationTime());

	// fix various issues with the game eW91dHViZS5jb20vZHlsYW5ob29r
	// these issues can only occur when a player is choking data.
	if (record->m_lag >= 2 && previous) {
		// detect fakewalk.
		float speed = record->m_velocity.length();

		if (record->m_flags & FL_ONGROUND
			&& record->m_layers[6].m_weight == 0.f
			&& record->m_lag > 2
			&& speed > 0.f
			&& speed <= record->max_speed * 0.34f)
			record->m_fake_walk = true;

		if (record->m_fake_walk) {
			record->m_velocity.Init();
			record->m_anim_velocity.Init();
		}

		bool bOnGround = record->m_flags & FL_ONGROUND;
		bool bJumped = false;
		bool bLandedOnServer = false;
		float flLandTime = 0.f;

		if (record->m_layers[4].m_cycle < 0.5f && (!(record->m_flags & FL_ONGROUND) || !(previous->m_flags & FL_ONGROUND))) {
			// note - VIO;
			// well i guess when llama wrote v3, he was drunk or sum cuz this is incorrect. -> cuz he changed this in v4.
			// and alpha didn't realize this but i did, so its fine.
			// improper way to do this -> flLandTime = record->m_flSimulationTime - float( record->m_serverAnimOverlays[ 4 ].m_flPlaybackRate * record->m_serverAnimOverlays[ 4 ].m_flCycle );
			// we need to divide instead of multiplication.
			flLandTime = record->m_sim_time - float(record->m_layers[4].m_playback_rate / record->m_layers[4].m_cycle);
			bLandedOnServer = flLandTime >= previous->m_sim_time;
		}

		if (bLandedOnServer && !bJumped) {
			if (flLandTime <= record->m_sim_time) {
				bJumped = true;
				bOnGround = true;
			}
			else {
				bOnGround = previous->m_flags & FL_ONGROUND;
			}
		}

		if (bOnGround) {
			this->m_player->m_fFlags() |= FL_ONGROUND;
		}
		else {
			this->m_player->m_fFlags() &= ~FL_ONGROUND;
		}

		// delta in duckamt and delta in time..
		float duck = record->m_duck - previous->m_duck;
		float time = record->m_sim_time - previous->m_sim_time;

		// get the duckamt change per tick.
		float change = (duck / time) * g_csgo.m_globals->m_interval;

		// fix crouching players.
		this->m_player->m_flDuckAmount() = previous->m_duck + change;
	}

	bool fake = !bot && g_menu.main.aimbot.enable.get();

	// if using fake angles, correct angles.
	if (fake)
		g_resolver.ResolveAngles(this->m_player, record);

	// set stuff before animating.
	this->m_player->m_vecOrigin() = record->m_origin;
	this->m_player->m_vecVelocity() = this->m_player->m_vecAbsVelocity() = record->m_anim_velocity;
	this->m_player->m_flLowerBodyYawTarget() = record->m_body;

	// EFL_DIRTY_ABSVELOCITY
	// skip call to C_BaseEntity::CalcAbsoluteVelocity
	this->m_player->m_iEFlags() &= ~(0x1000 | 0x800);

	// fix animating in same frame.
	if (state->m_last_update_frame >= g_csgo.m_globals->m_frame)
		state->m_last_update_frame = g_csgo.m_globals->m_frame - 1;

	// write potentially resolved angles.
	this->m_player->m_angEyeAngles() = record->m_eye_angles;

	// 'm_animating' returns true if being called from SetupVelocity, passes raw velocity to animstate.
	this->m_player->m_bClientSideAnimation() = true;
	this->m_player->UpdateClientSideAnimation();
	this->m_player->m_bClientSideAnimation() = false;

	// restore server layers
	this->m_player->SetAnimLayers(record->m_layers);

	// store updated/animated poses and rotation in lagrecord.
	this->m_player->GetPoseParameters(record->m_poses);
	record->m_abs_ang = this->m_player->GetAbsAngles();
	record->m_abs_ang.y = state->m_abs_yaw;

	// anims have updated.
	this->m_player->InvalidatePhysicsRecursive(InvalidatePhysicsBits_t::ANIMATION_CHANGED);
	this->m_player->InvalidatePhysicsRecursive(InvalidatePhysicsBits_t::ANGLES_CHANGED);
	this->m_player->InvalidatePhysicsRecursive(InvalidatePhysicsBits_t::SEQUENCE_CHANGED);

	// more accurate results due to global vars restoring/etc
	record->m_setup = g_bones.SetupBones(m_player, record->m_bones, 128, 0x7FF00, record->m_anim_time, record);

	// restore backup data.
	this->m_player->m_vecOrigin() = backup.m_origin;
	this->m_player->m_vecVelocity() = backup.m_velocity;
	this->m_player->m_vecAbsVelocity() = backup.m_abs_velocity;
	this->m_player->m_fFlags() = backup.m_flags;
	this->m_player->m_iEFlags() = backup.m_eflags;
	this->m_player->m_flDuckAmount() = backup.m_duck;
	this->m_player->m_flLowerBodyYawTarget() = backup.m_body;
	this->m_player->SetAbsOrigin(backup.m_abs_origin);

	// restore globals.
	g_csgo.m_globals->m_realtime = m_flRealtime;
	g_csgo.m_globals->m_curtime = m_flCurtime;
	g_csgo.m_globals->m_frametime = m_flFrametime;
	g_csgo.m_globals->m_abs_frametime = m_flAbsFrametime;
	g_csgo.m_globals->m_frame = m_iFramecount;
	g_csgo.m_globals->m_tick_count = m_iTickcount;
	g_csgo.m_globals->m_interp_amt = interpolation;
}

void AimPlayer::OnNetUpdate(Player* player) {
	bool reset = (!g_menu.main.aimbot.enable.get() || player->m_lifeState() == LIFE_DEAD || !player->enemy(g_cl.m_local));

	// if this happens, delete all the lagrecords.
	if (reset) {
		player->m_bClientSideAnimation() = true;
		this->m_records.clear();

		this->m_has_body_updated = false;
		this->m_fam_reverse_index = 0;
		this->m_airback_index = 0;
		this->m_fakewalk_index = 0;
		this->m_testfs_index = 0;
		this->m_lowlby_index = 0;
		this->m_logic_index = 0;
		this->m_stand_index4 = 0;
		this->m_air_index = 0;
		this->m_airlby_index = 0;
		this->m_broke_lby = false;
		this->m_spin_index = 0;
		this->m_stand_index1 = 0;
		this->m_stand_index2 = 0;
		this->m_stand_index3 = 0;
		this->m_edge_index = 0;
		this->m_back_index = 0;
		this->m_reversefs_index = 0;
		this->m_back_at_index = 0;
		this->m_reversefs_at_index = 0;
		this->m_lastmove_index = 0;
		this->m_lby_index = 0;
		this->m_airlast_index = 0;
		this->m_body_index = 0;
		this->m_lbyticks = 0;
		this->m_sidelast_index = 0;
		this->m_moving_index = 0;
		this->m_body_proxy_updated = false;

		return;
	}

	// update player ptr if required.
	// reset player if changed.
	if (this->m_player != player)
		this->m_records.clear();

	// update player ptr.
	this->m_player = player;

	// indicate that this player has been out of pvs.
	// insert dummy record to separate records
	// to fix stuff like animation and prediction.
	if (player->dormant()) {
		bool insert = true;

		// we have any records already?
		if (!this->m_records.empty()) {

			LagRecord* front = this->m_records.front().get();

			// we already have a dormancy separator.
			if (front->dormant())
				insert = false;
		}

		if (insert) {
			// add new record.
			this->m_records.emplace_front(std::make_shared< LagRecord >(player));

			// get reference to newly added record.
			LagRecord* current = m_records.front().get();

			// mark as dormant.
			current->m_dormant = true;
		}

		return;
	}

	bool update = (this->m_records.empty() || player->m_flSimulationTime() > this->m_records.front().get()->m_sim_time);

	// this is the first data update we are receving
	// OR we received data with a newer simulation context.
	if (update) {
		// add new record.
		m_records.emplace_front(std::make_shared< LagRecord >(player));

		// get reference to newly added record.
		LagRecord* current = this->m_records.front().get();

		// mark as non dormant.
		current->m_dormant = false;

		// update animations on current record.
		// call resolver.
		UpdateAnimations(current);

		// set shifting tickbase record.
		current->m_shift = game::TIME_TO_TICKS(current->m_sim_time) - g_csgo.m_globals->m_tick_count;
	}

	// no need to store insane amt of data.
	while (this->m_records.size() > 256)
		this->m_records.pop_back();
}

void AimPlayer::OnRoundStart(Player* player) {
	this->m_walk_record = {};
	this->m_player = player;
	this->m_shots = 0;
	this->m_missed_shots = 0;
	this->m_moved = false;
	this->m_body_update = FLT_MAX;
	this->m_test_index = 0;

	// reset stand and body index.
	this->m_has_body_updated = false;
	this->m_fam_reverse_index = 0;
	this->m_airback_index = 0;
	this->m_fakewalk_index = 0;
	this->m_testfs_index = 0;
	this->m_lowlby_index = 0;
	this->m_logic_index = 0;
	this->m_stand_index4 = 0;
	this->m_air_index = 0;
	this->m_airlby_index = 0;
	this->m_broke_lby = false;
	this->m_spin_index = 0;
	this->m_stand_index1 = 0;
	this->m_stand_index2 = 0;
	this->m_stand_index3 = 0;
	this->m_edge_index = 0;
	this->m_back_index = 0;
	this->m_reversefs_index = 0;
	this->m_back_at_index = 0;
	this->m_reversefs_at_index = 0;
	this->m_lastmove_index = 0;
	this->m_lby_index = 0;
	this->m_airlast_index = 0;
	this->m_body_index = 0;
	this->m_lbyticks = 0;
	this->m_sidelast_index = 0;
	this->m_moving_index = 0;
	this->m_body_proxy_updated = false;

	this->m_records.clear();
	this->m_hitboxes.clear();

	// IMPORTANT: DO NOT CLEAR LAST HIT SHIT.
}

void AimPlayer::SetupHitboxes(LagRecord* record) {
	// reset hitboxes.
	m_hitboxes.clear();
	//m_prefer_body = false;

	if (g_cl.m_weapon_id == ZEUS) {
		// hitboxes for the zeus.
		m_hitboxes.push_back({ HITBOX_BODY });
		m_hitboxes.push_back({ HITBOX_CHEST });
		return;
	}

	//m_prefer_body = true;

	// only, on key.
	MultiDropdown hitbox{ g_menu.main.aimbot.hitbox };

	// head
	if (hitbox.get(0)) {
		m_hitboxes.push_back({ HITBOX_HEAD });
	}

	// stomach and pelvis
	if (hitbox.get(2)) {
		m_hitboxes.push_back({ HITBOX_BODY });
		m_hitboxes.push_back({ HITBOX_PELVIS });
	}

	// chest
	if (hitbox.get(1)) {
		m_hitboxes.push_back({ HITBOX_CHEST });
		m_hitboxes.push_back({ HITBOX_UPPER_CHEST });
	}

	if (hitbox.get(3)) {
		m_hitboxes.push_back({ HITBOX_L_UPPER_ARM });
		m_hitboxes.push_back({ HITBOX_R_UPPER_ARM });
	}

	// legs
	if (hitbox.get(4)) {
		m_hitboxes.push_back({ HITBOX_L_CALF });
		m_hitboxes.push_back({ HITBOX_R_CALF });

		m_hitboxes.push_back({ HITBOX_L_THIGH });
		m_hitboxes.push_back({ HITBOX_R_THIGH });
	}

	// feet
	if (hitbox.get(5)) {
		m_hitboxes.push_back({ HITBOX_L_FOOT });
		m_hitboxes.push_back({ HITBOX_R_FOOT });
	}
}
void Aimbot::init() {
	// clear old targets.
	m_targets.clear();

	m_target = nullptr;
	m_aim = vec3_t{ };
	m_angle = ang_t{ };
	m_damage = 0.f;
	m_record = nullptr;
	m_stop = false;

	m_best_dist = std::numeric_limits< float >::max();
	m_best_fov = 180.f + 1.f;
	m_best_damage = 0.f;
	m_best_hp = 100 + 1;
	m_best_lag = std::numeric_limits< float >::max();
	m_best_height = std::numeric_limits< float >::max();
}

void Aimbot::StripAttack() {
	if (g_cl.m_weapon_id == REVOLVER)
		g_cl.m_cmd->m_buttons &= ~IN_ATTACK2;

	else
		g_cl.m_cmd->m_buttons &= ~IN_ATTACK;
}

void Aimbot::think() {
	// do all startup routines.
	init();

	// sanity.
	if (!g_cl.m_weapon)
		return;

	// no grenades or bomb.
	if (g_cl.m_weapon_type == WEAPONTYPE_GRENADE || g_cl.m_weapon_type == WEAPONTYPE_C4)
		return;

	if (!g_cl.m_weapon_fire)
		StripAttack();

	// we have no aimbot enabled.
	if (!g_menu.main.aimbot.enable.get())
		return;

	// animation silent aim, prevent the ticks with the shot in it to become the tick that gets processed.
	// we can do this by always choking the tick before we are able to shoot.
	bool revolver = g_cl.m_weapon_id == REVOLVER && g_cl.m_revolver_cock != 0;

	// one tick before being able to shoot.
	if (revolver && g_cl.m_revolver_cock > 0 && g_cl.m_revolver_cock == g_cl.m_revolver_query) {
		*g_cl.m_packet = false;
		return;
	}

	// we have a normal weapon or a non cocking revolver
	// choke if its the processing tick.
	if (g_cl.m_weapon_fire && !g_cl.m_lag && !revolver) {
		*g_cl.m_packet = false;
		StripAttack();
		return;
	}

	// no point in aimbotting if we cannot fire this tick.
	if (!g_cl.m_weapon_fire)
		return;

	// setup bones for all valid targets.
	for (int i{ 1 }; i <= g_csgo.m_globals->m_max_clients; ++i) {
		Player* player = g_csgo.m_entlist->GetClientEntity< Player* >(i);

		if (!IsValidTarget(player))
			continue;

		AimPlayer* data = &m_players[i - 1];
		if (!data)
			continue;

		// store player as potential target this tick.
		m_targets.emplace_back(data);
	}

	// run knifebot.
	if (g_cl.m_weapon_type == WEAPONTYPE_KNIFE && g_cl.m_weapon_id != ZEUS) {

		knife();

		return;
	}

	// scan available targets... if we even have any.
	find();

	// finally set data when shooting.
	apply();
}

//void Aimbot::find() {
//	struct BestTarget_t { Player* player; vec3_t pos; float damage; LagRecord* record; int hitbox; int hitgroup; };
//
//	vec3_t       tmp_pos;
//	float        tmp_damage;
//	int          tmp_hitbox, tmp_hitgroup;
//	BestTarget_t best;
//	best.player = nullptr;
//	best.damage = -1.f;
//	best.pos = vec3_t{ };
//	best.record = nullptr;
//	best.hitbox = -1;
//	best.hitgroup = -1;
//
//	if (m_targets.empty())
//		return;
//
//	if (g_cl.m_weapon_id == ZEUS && !g_menu.main.aimbot.zeusbot.get())
//		return;
//
//	// iterate all targets.
//	for (const auto& t : m_targets) {
//		if (t->m_records.empty())
//			continue;
//
//		// this player broke lagcomp.
//		// his bones have been resetup by our lagcomp.
//		// therfore now only the front record is valid.
//		if (g_menu.main.aimbot.lagfix.get() && g_lagcomp.StartPrediction(t)) {
//			LagRecord* front = t->m_records.front().get();
//
//			t->SetupHitboxes(front, false);
//			if (t->m_hitboxes.empty())
//				continue;
//
//			// rip something went wrong..
//			if (t->GetBestAimPosition(tmp_pos, tmp_damage, front, tmp_hitbox, tmp_hitgroup) && SelectTarget(front, tmp_pos, tmp_damage)) {
//
//				// if we made it so far, set shit.
//				best.player = t->m_player;
//				best.pos = tmp_pos;
//				best.damage = tmp_damage;
//				best.record = front;
//				best.hitbox = tmp_hitbox;
//				best.hitgroup = tmp_hitgroup;
//			}
//		}
//
//		// player did not break lagcomp.
//		// history aim is possible at this point.
//		else {
//			LagRecord* ideal = g_resolver.FindIdealRecord(t);
//			if (!ideal)
//				continue;
//
//			t->SetupHitboxes(ideal, false);
//			if (t->m_hitboxes.empty())
//				continue;
//
//			// try to select best record as target.
//			if (t->GetBestAimPosition(tmp_pos, tmp_damage, ideal, tmp_hitbox, tmp_hitgroup) && SelectTarget(ideal, tmp_pos, tmp_damage)) {
//				// if we made it so far, set shit.
//				best.player = t->m_player;
//				best.pos = tmp_pos;
//				best.damage = tmp_damage;
//				best.record = ideal;
//				best.hitbox = tmp_hitbox;
//				best.hitgroup = tmp_hitgroup;
//			}
//
//			LagRecord* last = g_resolver.FindLastRecord(t);
//			if (!last || last == ideal)
//				continue;
//
//			t->SetupHitboxes(last, true);
//			if (t->m_hitboxes.empty())
//				continue;
//
//			// rip something went wrong..
//			if (t->GetBestAimPosition(tmp_pos, tmp_damage, last, tmp_hitbox, tmp_hitgroup) && SelectTarget(last, tmp_pos, tmp_damage)) {
//				// if we made it so far, set shit.
//				best.player = t->m_player;
//				best.pos = tmp_pos;
//				best.damage = tmp_damage;
//				best.record = last;
//				best.hitbox = tmp_hitbox;
//				best.hitgroup = tmp_hitgroup;
//			}
//		}
//	}
//
//	// verify our target and set needed data.
//	if (best.player && best.record) {
//		// calculate aim angle.
//		update_shoot_pos();
//
//		// calculate aim angle.
//		math::VectorAngles(best.pos - g_cl.m_shoot_pos, m_angle);
//
//		// set member vars.
//		m_target = best.player;
//		m_aim = best.pos;
//		m_damage = best.damage;
//		m_record = best.record;
//		m_hitbox = best.hitbox;
//		m_hitgroup = best.hitgroup;
//
//		// write data, needed for traces / etc.
//		m_record->cache();
//
//		// set autostop shit.
//		m_stop = !(g_cl.m_buttons & IN_JUMP);
//
//		bool on = g_menu.main.aimbot.hitchance.get() && g_menu.main.config.mode.get() == 0;
//		bool hit = on && CheckHitchance(m_target, m_angle);
//
//		// if we can scope.
//		bool can_scope = !g_cl.m_local->m_bIsScoped() && (g_cl.m_weapon_id == AUG || g_cl.m_weapon_id == SG553 || g_cl.m_weapon_type == WEAPONTYPE_SNIPER_RIFLE);
//
//		if (can_scope) {
//			// always.
//			if (g_menu.main.aimbot.zoom.get() == 1) {
//				g_cl.m_cmd->m_buttons |= IN_ATTACK2;
//				return;
//			}
//
//			// hitchance fail.
//			else if (g_menu.main.aimbot.zoom.get() == 2 && on && !hit) {
//				g_cl.m_cmd->m_buttons |= IN_ATTACK2;
//				return;
//			}
//		}
//
//		if (hit || !on) {
//			// right click attack.
//			if (g_menu.main.config.mode.get() == 1 && g_cl.m_weapon_id == REVOLVER)
//				g_cl.m_cmd->m_buttons |= IN_ATTACK2;
//
//			// left click attack.
//			else
//				g_cl.m_cmd->m_buttons |= IN_ATTACK;
//		}
//	}
//}

void Aimbot::find() {
	struct BestTarget_t { Player* player{}; vec3_t pos{}; float damage{}; int hitbox{}; int hitgroup{}; LagRecord* record{}; };

	vec3_t       tmp_pos;
	float        tmp_damage;
	int			 tmp_hitbox{}, tmp_hitgroup{};
	BestTarget_t best;
	float best_health = 999.f;
	best.player = nullptr;
	best.damage = -1.f;
	best.pos = vec3_t{ };
	best.record = nullptr;
	best.hitbox = -1;
	best.hitgroup = -1;

	if (m_targets.empty())
		return;

	// iterate all targets.
	for (const auto& t : m_targets) {
		if (t->m_records.empty())
			continue;

		// set em up
		t->SetupHitboxes(nullptr);

		// lol.. what hitbox are you gonna shoot at?
		if (t->m_hitboxes.empty())
			continue;

		const int pred = g_lagcomp.StartPrediction(t);

		// this player broke lagcomp.
		// his bones have been resetup by our lagcomp.
		// therfore now only the front record is valid.
		if (pred != RETURN_NO_LC) {
			if (pred == RETURN_DELAY)
				continue;

			LagRecord* front = t->m_records.front().get();

			// rip something went wrong..
			if (t->GetBestAimPosition(tmp_pos, tmp_damage, front, tmp_hitbox, tmp_hitgroup) && SelectTarget(front, tmp_pos, tmp_damage)) {

				// if we made it so far, set shit.
				best.player = t->m_player;
				best.pos = tmp_pos;
				best.damage = tmp_damage;
				best.record = front;
				best.hitbox = tmp_hitbox;
				best.hitgroup = tmp_hitgroup;
			}

			// breaking lc
			continue;
		}

		LagRecord* ideal = g_resolver.FindIdealRecord(t);
		if (!ideal)
			continue;

			if (t->GetBestAimPosition(tmp_pos, tmp_damage, ideal, tmp_hitbox, tmp_hitgroup) && SelectTarget(ideal, tmp_pos, tmp_damage)) {
				// if we made it so far, set shit.
				best.player = t->m_player;
				best.pos = tmp_pos;
				best.damage = tmp_damage;
				best.record = ideal;
				best.hitbox = tmp_hitbox;
				best.hitgroup = tmp_hitgroup;
			}

		// last record
		LagRecord* last = g_resolver.FindLastRecord(t);
		if (!last
			|| last == ideal)
			continue;

		// rip something went wrong..
		if (t->GetBestAimPosition(tmp_pos, tmp_damage, last, tmp_hitbox, tmp_hitgroup) && SelectTarget(last, tmp_pos, tmp_damage)) {
			// if we made it so far, set shit.
			best.player = t->m_player;
			best.pos = tmp_pos;
			best.damage = tmp_damage;
			best.record = last;
			best.hitbox = tmp_hitbox;
			best.hitgroup = tmp_hitgroup;
		}
	}

	// verify our target and set needed data.
	if (best.player && best.record) {
		// calculate aim angle.
		math::VectorAngles(best.pos - g_cl.m_shoot_pos, m_angle);

		// set member vars.
		m_target = best.player;
		m_aim = best.pos;
		m_damage = best.damage;
		m_record = best.record;
		m_hitbox = best.hitbox;
		m_hitgroup = best.hitgroup;

		// write data, needed for traces / etc.
		m_record->cache();

		// set autostop shit.
		if (g_cl.m_local->m_fFlags() & FL_ONGROUND
			&& !(g_cl.m_buttons & IN_JUMP) && !(g_cl.m_weapon_id == ZEUS)) {
			if (g_cl.m_weapon_fire || (g_cl.m_player_fire))
				m_stop = true;
		}

		bool on = g_menu.main.aimbot.enable.get() && g_menu.main.config.mode.get() == 0;
		bool hit = on && CheckHitchance(m_target, m_angle);

		// if we can scope.
		bool can_scope = !g_cl.m_local->m_bIsScoped() && (g_cl.m_weapon_id == AUG || g_cl.m_weapon_id == SG553 || g_cl.m_weapon_type == WEAPONTYPE_SNIPER_RIFLE);


		if (hit || !on) {
			// right click attack.
			if (g_menu.main.config.mode.get() == 1 && g_cl.m_weapon_id == REVOLVER)
				g_cl.m_cmd->m_buttons |= IN_ATTACK2;

			// left click attack.
			else
				g_cl.m_cmd->m_buttons |= IN_ATTACK;
		}
	}
}


// @evitable: use this it's better and more accurate.
// @evitable: s/o kaaba.su <3
bool Aimbot::CanHitPlayer(LagRecord* pRecord, const vec3_t& vecEyePos, const vec3_t& vecEnd, int iHitboxIndex) {
	auto hdr = *(studiohdr_t**)pRecord->m_player->m_studioHdr();
	if (!hdr)
		return false;

	auto pHitboxSet = hdr->GetHitboxSet(pRecord->m_player->m_nHitboxSet());

	if (!pHitboxSet)
		return false;

	auto pHitbox = pHitboxSet->GetHitbox(iHitboxIndex);

	if (!pHitbox)
		return false;

	const auto pMatrix = pRecord->m_bones;
	if (!pMatrix)
		return false;

	const vec3_t vecServerImpact = vec3_t{ g_visuals.m_impacts.back().x, g_visuals.m_impacts.back().y, g_visuals.m_impacts.back().z };

	const float flRadius = pHitbox->m_radius;
	const bool bCapsule = flRadius != -1.f;

	vec3_t vecMin;
	vec3_t vecMax;

	math::VectorTransform(pHitbox->m_mins, pRecord->m_bones[pHitbox->m_bone], vecMin);
	math::VectorTransform(pHitbox->m_maxs, pRecord->m_bones[pHitbox->m_bone], vecMax);

	auto dir = vecEnd - vecEyePos;
	dir.normalize();

	const bool bIntersected = bCapsule ? math::IntersectSegmentToSegment(vecEyePos, vecServerImpact, vecMin, vecMax, flRadius) : math::IntersectionBoundingBox(vecEyePos, vecEnd, vecMin, vecMax);

	return bIntersected;
};

bool Aimbot::CheckHitchance(Player* player, const ang_t& angle)
{
	constexpr float HITCHANCE_MAX = 100.f;
	int   SEED_MAX = 256;
	const float accurate_speed = std::floor((g_cl.m_local->m_bIsScoped() ? g_cl.m_weapon_info->m_max_player_speed_alt : g_cl.m_weapon_info->m_max_player_speed) * 0.33f);

	// force accuracy
	if (g_cl.m_local->m_vecVelocity().length_2d() >= accurate_speed)
		return false;

	if (g_cl.m_local->m_flDuckAmount() >= 0.125f) {
		if (g_cl.m_flPreviousDuckAmount > g_cl.m_local->m_flDuckAmount()) {
			return false;
		}
	}

	vec3_t     start{ g_cl.m_shoot_pos }, end, fwd, right, up, dir, wep_spread;
	float      inaccuracy, spread;
	CGameTrace tr;
	size_t     total_hits{ }, needed_hits{ (size_t)std::ceil((g_menu.main.aimbot.hitchance_amount.get() * SEED_MAX) / HITCHANCE_MAX) };

	math::AngleVectors(angle, &fwd, &right, &up);

	inaccuracy = g_cl.m_weapon->GetInaccuracy();
	spread = g_cl.m_weapon->GetSpread();
	float recoil_index = g_cl.m_weapon->m_flRecoilIndex();

	const vec3_t backup_origin = player->m_vecOrigin();
	const vec3_t backup_abs_origin = player->GetAbsOrigin();
	const ang_t backup_abs_angles = player->GetAbsAngles();
	const vec3_t backup_obb_mins = player->m_vecMins();
	const vec3_t backup_obb_maxs = player->m_vecMaxs();

	auto restore = [&]() -> void {
		player->m_vecOrigin() = backup_origin;
		player->SetAbsOrigin(backup_abs_origin);
		player->SetAbsAngles(backup_abs_angles);
		player->m_vecMins() = backup_obb_mins;
		player->m_vecMaxs() = backup_obb_maxs;
		};

	for (int i{ 0 }; i <= SEED_MAX; ++i) {
		wep_spread = g_cl.m_weapon->CalculateSpread(i, inaccuracy, spread);

		dir = (fwd + (right * wep_spread.x) + (up * wep_spread.y)).normalized();

		end = start + (dir * g_cl.m_weapon_info->m_range);

		player->m_vecOrigin() = m_record->m_pred_origin;
		player->SetAbsOrigin(m_record->m_pred_origin);
		player->SetAbsAngles(m_record->m_abs_ang);
		player->m_vecMins() = m_record->m_mins;
		player->m_vecMaxs() = m_record->m_maxs;

		g_csgo.m_engine_trace->ClipRayToEntity(Ray(start, end), MASK_SHOT, player, &tr);

		restore();

		if (tr.m_entity == player && game::IsValidHitgroup(tr.m_hitgroup))
			++total_hits;

		if (total_hits >= needed_hits)
			return true;

		if ((SEED_MAX - i + total_hits) < needed_hits)
			return false;
	}

	for (int i{ 0 }; i <= SEED_MAX; ++i) {
		float a = g_csgo.RandomFloat(0.f, 1.f);
		float b = g_csgo.RandomFloat(0.f, math::pi * 2.f);
		float c = g_csgo.RandomFloat(0.f, 1.f);
		float d = g_csgo.RandomFloat(0.f, math::pi * 2.f);

		if (g_cl.m_weapon_id == REVOLVER) {
			a = 1.f - a * a;
			a = 1.f - c * c;
		}
		else if (g_cl.m_weapon_id == NEGEV && recoil_index < 3.0f) {
			for (int i = 3; i > recoil_index; i--) {
				a *= a;
				c *= c;
			}

			a = 1.0f - a;
			c = 1.0f - c;
		}

		float inac = a * inaccuracy;
		float sir = c * spread;

		vec3_t sirVec((cos(b) * inac) + (cos(d) * sir), (sin(b) * inac) + (sin(d) * sir), 0), direction;

		direction.x = fwd.x + (sirVec.x * right.x) + (sirVec.y * up.x);
		direction.y = fwd.y + (sirVec.x * right.y) + (sirVec.y * up.y);
		direction.z = fwd.z + (sirVec.x * right.z) + (sirVec.y * up.z);
		direction.normalize();

		ang_t viewAnglesSpread;
		math::VectorAngles(direction, viewAnglesSpread, &up);
		viewAnglesSpread.normalize();

		vec3_t viewForward;
		math::AngleVectors(viewAnglesSpread, &viewForward);
		viewForward.normalized();

		end = start + (viewForward * g_cl.m_weapon_info->m_range);

		player->m_vecOrigin() = m_record->m_pred_origin;
		player->SetAbsOrigin(m_record->m_pred_origin);
		player->SetAbsAngles(m_record->m_abs_ang);
		player->m_vecMins() = m_record->m_mins;
		player->m_vecMaxs() = m_record->m_maxs;

		g_csgo.m_engine_trace->ClipRayToEntity(Ray(start, end), MASK_SHOT, player, &tr);

		restore();

		if (tr.m_entity == player && game::IsValidHitgroup(tr.m_hitgroup))
			++total_hits;

		if (total_hits >= needed_hits)
			return true;

		if ((SEED_MAX - i + total_hits) < needed_hits)
			return false;
	}
}


bool AimPlayer::SetupHitboxPoints(LagRecord* record, BoneArray* bones, int index, std::vector< vec3_t >& points) {
	// reset points.
	points.clear();

	const model_t* model = m_player->GetModel();
	if (!model)
		return false;

	studiohdr_t* hdr = g_csgo.m_model_info->GetStudioModel(model);
	if (!hdr)
		return false;

	mstudiohitboxset_t* set = hdr->GetHitboxSet(m_player->m_nHitboxSet());
	if (!set)
		return false;

	mstudiobbox_t* bbox = set->GetHitbox(index);
	if (!bbox)
		return false;

	// get hitbox scales.
	float scale = g_menu.main.aimbot.scale.get() / 100.f;

	// big inair fix.
	if (!(record->m_pred_flags & FL_ONGROUND) && scale > 0.7f)
		scale = 0.7f;

	float bscale = g_menu.main.aimbot.seperate_pointscale.get() ? g_menu.main.aimbot.body_scale.get() / 100.f : g_menu.main.aimbot.scale.get() / 100.f;


	// big duck fix.
	//if (!(record->m_pred_flags & FL_DUCKING) && bscale > 0.6f)
	//	bscale = 0.6f;

	// these indexes represent boxes.
	if (bbox->m_radius <= 0.f) {
		// references: 
		//      https://developer.valvesoftware.com/wiki/Rotation_Tutorial
		//      CBaseAnimating::GetHitboxBonePosition
		//      CBaseAnimating::DrawServerHitboxes

		// convert rotation angle to a matrix.
		matrix3x4_t rot_matrix;
		g_csgo.AngleMatrix(bbox->m_angle, rot_matrix);

		// apply the rotation to the entity input space ( local ).
		matrix3x4_t matrix;
		math::ConcatTransforms(record->m_bones[bbox->m_bone], rot_matrix, matrix);

		// extract origin from matrix.
		vec3_t origin = matrix.GetOrigin();

		// compute raw center point.
		vec3_t center = (bbox->m_mins + bbox->m_maxs) / 2.f;

		// the feet hiboxes have a side, heel and the toe.
		if (index == HITBOX_R_FOOT || index == HITBOX_L_FOOT) {

			// side is more optimal then center.
			points.push_back({ center.x, center.y, center.z });

			if (g_menu.main.aimbot.multipoint.get() == 2) {

				// get point offset relative to center point
				// and factor in hitbox scale.
				float d2 = (bbox->m_mins.x - center.x) * scale;

				// heel.
				points.push_back({ center.x + d2, center.y, center.z });
			}
		}

		// nothing to do here we are done.
		if (points.empty())
			return false;

		// rotate our bbox points by their correct angle
		// and convert our points to world space.
		for (auto& p : points) {
			// VectorRotate.
			// rotate point by angle stored in matrix.
			p = { p.dot(matrix[0]), p.dot(matrix[1]), p.dot(matrix[2]) };

			// transform point to world space.
			p += origin;
		}
	}

	// these hitboxes are capsules.
	else {
		// factor in the pointscale.
		float r = bbox->m_radius * scale;
		float br = bbox->m_radius * bscale;

		// compute raw center point.
		vec3_t center = (bbox->m_mins + bbox->m_maxs) / 2.f;

		// head has 5 points.
		if (index == HITBOX_HEAD) {
			// add center.
			points.push_back(center);

			if (g_menu.main.aimbot.multipoint.get() == 0 || g_menu.main.aimbot.multipoint.get() == 1 || g_menu.main.aimbot.multipoint.get() == 2) {
				// rotation matrix 45 degrees.
				// https://math.stackexchange.com/questions/383321/rotating-x-y-points-45-degrees
				// std::cos( deg_to_rad( 45.f ) )
				constexpr float rotation = 0.70710678f;

				// top/back 45 deg.
				// this is the best spot to shoot at.
				points.push_back({ bbox->m_maxs.x + (rotation * r), bbox->m_maxs.y + (-rotation * r), bbox->m_maxs.z });

				// right.
				points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + r });

				// left.
				points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - r });

				// back.
				points.push_back({ bbox->m_maxs.x, bbox->m_maxs.y - r, bbox->m_maxs.z });

				// get animstate ptr.
				CCSGOPlayerAnimState* state = record->m_player->m_PlayerAnimState();

				if (state && record->m_anim_velocity.length() <= 0.1f && record->m_eye_angles.x <= state->m_aim_pitch_min) {

					// bottom point.
					points.push_back({ bbox->m_maxs.x - r, bbox->m_maxs.y, bbox->m_maxs.z });
				}
			}
		}

		// body has 4 points
		// center + back + left + right
		else if (index == HITBOX_PELVIS) {
			if (g_menu.main.aimbot.multipoint.get() == 0 || g_menu.main.aimbot.multipoint.get() == 1 || g_menu.main.aimbot.multipoint.get() == 2) {
				points.push_back({ center.x, center.y, bbox->m_maxs.z + br });
				points.push_back({ center.x, center.y, bbox->m_mins.z - br });
				points.push_back({ center.x, bbox->m_maxs.y - br, center.z });
			}
		}
		else if (index == HITBOX_CHEST) {
			if (g_menu.main.aimbot.multipoint.get() == 1 || g_menu.main.aimbot.multipoint.get() == 2) {
				points.push_back({ center.x, center.y, bbox->m_maxs.z + br });
				points.push_back({ center.x, center.y, bbox->m_mins.z - br });
				points.push_back({ center.x, bbox->m_maxs.y - br, center.z });
			}
		}
		// exact same as pelvis but inverted ( god knows what theyre doing at valve )
		else if (index == HITBOX_BODY) {
			points.push_back(center);

			if (g_menu.main.aimbot.multipoint.get() == 0 || g_menu.main.aimbot.multipoint.get() == 1 || g_menu.main.aimbot.multipoint.get() == 2) {
				points.push_back({ center.x, bbox->m_maxs.y - br, center.z });
			}
		}

		// other stomach/chest hitboxes have 3 points.
		else if (index == HITBOX_THORAX) {
			// add center.
			points.push_back(center);

			// add extra point on back.
			if (g_menu.main.aimbot.multipoint.get() == 1 || g_menu.main.aimbot.multipoint.get() == 2) {
				points.push_back({ center.x, bbox->m_maxs.y - br, center.z });
			}
		}

		else if (index == HITBOX_R_CALF || index == HITBOX_L_CALF) {
			// add center.
			points.push_back(center);

			// half bottom.
			if (g_menu.main.aimbot.multipoint.get() == 2)
				points.push_back({ bbox->m_maxs.x - (bbox->m_radius / 2.f), bbox->m_maxs.y, bbox->m_maxs.z });
		}

		else if (index == HITBOX_R_THIGH || index == HITBOX_L_THIGH) {
			// add center.
			points.push_back(center);
		}

		// arms get only one point.
		else if (index == HITBOX_R_UPPER_ARM || index == HITBOX_L_UPPER_ARM) {
			// elbow.
			points.push_back({ bbox->m_maxs.x + bbox->m_radius, center.y, center.z });
		}

		// nothing left to do here.
		if (points.empty())
			return false;

		// transform capsule points.`
		for (auto& p : points)
			math::VectorTransform(p, record->m_bones[bbox->m_bone], p);
	}

	return true;
}

bool AimPlayer::GetBestAimPosition(vec3_t& aim, float& damage, LagRecord* record, int& hitbox, int& hitgroup) {
	bool                  done, pen;
	float                 dmg;
	HitscanData_t         scan;
	std::vector< vec3_t > points;

	// get player hp.
	int hp = std::min(100, m_player->m_iHealth());

	if (g_cl.m_weapon_id == ZEUS) {
		dmg = hp;
		pen = false;
	}

	else {
		dmg = g_aimbot.m_damage_toggle ? g_menu.main.aimbot.override_dmg_value.get() : g_menu.main.aimbot.minimal_damage.get();
		if (g_menu.main.aimbot.minimal_damage_hp.get())
			dmg = std::ceil((dmg / 100.f) * hp);

		pen = true;
	}

	// write all data of this record l0l.
	record->cache();

	// get max body dmg for current weapon
	float max_body_dmg = penetration::scale(m_player, g_cl.m_weapon_info->m_damage, g_cl.m_weapon_info->m_armor_ratio, HITGROUP_STOMACH);

	// iterate hitboxes.
	for (const auto& it : m_hitboxes) {
		done = false;

		// setup points on hitbox.
		if (!SetupHitboxPoints(record, record->m_bones, it.m_index, points))
			continue;

		// iterate points on hitbox.
		for (const auto& point : points) {
			penetration::PenetrationInput_t in;

			in.m_damage = dmg;
			in.m_damage_pen = dmg;
			in.m_can_pen = pen;
			in.m_target = m_player;
			in.m_from = g_cl.m_local;
			in.m_pos = point;

			penetration::PenetrationOutput_t out;

			// we can hit p!
			if (penetration::run(&in, &out)) {

				// this hitbox requires lethality to get selected, if that is the case.
				// we are done, stop now.
				if (out.m_hitgroup != HITGROUP_HEAD
					&& out.m_damage >= m_player->m_iHealth()
					)
					done = true;

				// this is the highest body damage we can deal, stop here
				else if (it.m_index > 2
					&& out.m_damage >= std::floor(max_body_dmg - 3.f)) // lets compensate for distance n such
					done = true;

				// prefered hitbox, just stop now.
				if (it.m_mode == HitscanMode::PREFER)
					done = true;

				// this hitbox has normal selection, it needs to have more damage.
				else if (it.m_mode == HitscanMode::NORMAL) {
					// we did more damage.
					if (out.m_damage > scan.m_damage) {
						// save new best data.
						scan.m_damage = out.m_damage;
						scan.m_pos = point;
						scan.m_hitbox = it.m_index;
						scan.m_hitgroup = out.m_hitgroup;

						// if the first point is lethal
						// screw the other ones.
						if (point == points.front() && out.m_damage >= m_player->m_iHealth())
							break;
					}
				}

				// we found a preferred / lethal hitbox.
				if (done) {
					// save new best data.
					scan.m_damage = out.m_damage;
					scan.m_pos = point;
					scan.m_hitbox = it.m_index;
					scan.m_hitgroup = out.m_hitgroup;
					break;
				}
			}
		}

		// ghetto break out of outer loop.
		if (done)
			break;
	}

	// we found something that we can damage.
	// set out vars.
	if (scan.m_damage > 0.f) {
		aim = scan.m_pos;
		damage = scan.m_damage;
		hitbox = scan.m_hitbox;
		hitgroup = scan.m_hitgroup;
		return true;
	}

	return false;
}

bool Aimbot::SelectTarget(LagRecord* record, const vec3_t& aim, float damage) {
	float dist, fov, height;
	int   hp;

	// fov check.
	//if (g_menu.main.aimbot.fov.get()) {
	//	// if out of fov, retn false.
	//	if (math::GetFOV(g_cl.m_view_angles, g_cl.m_shoot_pos, aim) > g_menu.main.aimbot.fov_amount.get())
	//		return false;
	//}

		if (damage > m_best_damage) {
			m_best_damage = damage;
			return true;
		}

	return false;
}

void Aimbot::apply() {
	bool attack, attack2;
	Color color;
	// attack states.
	attack = (g_cl.m_cmd->m_buttons & IN_ATTACK);
	attack2 = (g_cl.m_weapon_id == REVOLVER && g_cl.m_cmd->m_buttons & IN_ATTACK2);

	// ensure we're attacking.
	if (attack || attack2) {
		// choke every shot.
		*g_cl.m_packet = g_cl.m_lag >= 14;

		if (m_target) {
			// make sure to aim at un-interpolated data.
			// do this so BacktrackEntity selects the exact record.
			if (m_record && !m_record->m_broke_lc)
				g_cl.m_cmd->m_tick = game::TIME_TO_TICKS(m_record->m_sim_time + g_cl.m_lerp);

			// set angles to target.
			g_cl.m_cmd->m_view_angles = m_angle;

			// if not silent aim, apply the viewangles.
			if (!g_menu.main.aimbot.silent.get())
				g_csgo.m_engine->SetViewAngles(m_angle);

			// store fired shot.
			g_shots.OnShotFire(m_target, m_damage, g_cl.m_weapon_info->m_bullets, m_record, m_hitbox, m_hitgroup, m_aim);

			// draw aimbot matrix.
			g_visuals.DrawHitboxMatrix(m_record, 3.f);
		}

		// nospread.
		if (g_menu.main.aimbot.nospread.get() && g_menu.main.config.mode.get() == 1)
			NoSpread();

		// norecoil.
		if (g_menu.main.aimbot.norecoil.get())
			g_cl.m_cmd->m_view_angles -= g_cl.m_local->m_aimPunchAngle() * g_csgo.weapon_recoil_scale->GetFloat();

		// set that we fired.
		g_cl.m_shot = true;
	}
}

void Aimbot::NoSpread() {
	bool    attack2;
	vec3_t  spread, forward, right, up, dir;

	// revolver state.
	attack2 = (g_cl.m_weapon_id == REVOLVER && (g_cl.m_cmd->m_buttons & IN_ATTACK2));

	// get spread.
	spread = g_cl.m_weapon->CalculateSpread(g_cl.m_cmd->m_random_seed, attack2);

	// compensate.
	g_cl.m_cmd->m_view_angles -= { -math::rad_to_deg(std::atan(spread.length_2d())), 0.f, math::rad_to_deg(std::atan2(spread.x, spread.y)) };
}