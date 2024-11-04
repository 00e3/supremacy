#include "includes.h"

Listener g_listener{};;

void events::round_start(IGameEvent* evt) {
	// new round has started. no longer round end.
	g_cl.m_round_end = false;

	// fix fix?
	g_cl.m_body_pred = g_csgo.m_globals->m_curtime;

	// remove notices.
	if (g_menu.main.misc.killfeed.get()) {
		KillFeed_t* feed = (KillFeed_t*)g_csgo.m_hud->FindElement(HASH("SFHudDeathNoticeAndBotStatus"));
		if (feed)
			g_csgo.ClearNotices(feed);
	}

	// reset hvh / aa stuff.
	g_hvh.m_next_random_update = 0.f;
	g_hvh.m_auto_last = 0.f;

	// reset bomb stuff.
	g_visuals.m_c4_planted = false;
	g_visuals.m_planted_c4 = nullptr;
	g_visuals.bombexploded = false;
	g_visuals.bombedefused = false;

	// reset dormant esp.
	g_visuals.m_draw.fill(false);
	g_visuals.m_opacities.fill(0.f);
	g_visuals.m_offscreen_damage.fill(OffScreenDamageData_t());

	// buybot.
	{
		auto buy1 = g_menu.main.misc.buy1.GetActiveItems();
		auto buy2 = g_menu.main.misc.buy2.GetActiveItems();
		auto buy3 = g_menu.main.misc.buy3.GetActiveItems();

		for (auto it = buy1.begin(); it != buy1.end(); ++it)
			g_csgo.m_engine->ExecuteClientCmd(tfm::format(XOR("buy %s"), *it).data());

		for (auto it = buy2.begin(); it != buy2.end(); ++it)
			g_csgo.m_engine->ExecuteClientCmd(tfm::format(XOR("buy %s"), *it).data());

		for (auto it = buy3.begin(); it != buy3.end(); ++it)
			g_csgo.m_engine->ExecuteClientCmd(tfm::format(XOR("buy %s"), *it).data());
	}

	// update all players.
	for (int i{ 1 }; i <= g_csgo.m_globals->m_max_clients; ++i) {
		Player* player = g_csgo.m_entlist->GetClientEntity< Player* >(i);
		if (!player || player->m_bIsLocalPlayer())
			continue;

		AimPlayer* data = &g_aimbot.m_players[i - 1];
		data->OnRoundStart(player);
	}

	// clear origins.
	g_cl.m_net_pos.clear();
}

void events::round_end(IGameEvent* evt) {
	if (!g_cl.m_local)
		return;

	// get the reason for the round end.
	int reason = evt->m_keys->FindKey(HASH("reason"))->GetInt();

	// reset.
	g_cl.m_round_end = false;

	if (g_cl.m_local->m_iTeamNum() == TEAM_COUNTERTERRORISTS && reason == CSRoundEndReason::CT_WIN)
		g_cl.m_round_end = true;

	else if (g_cl.m_local->m_iTeamNum() == TEAM_TERRORISTS && reason == CSRoundEndReason::T_WIN)
		g_cl.m_round_end = true;
}

void events::player_hurt(IGameEvent* evt) {
	Player* attacker = nullptr;
	Player* victim = nullptr;

	// forward event to shots hurt processing.
	g_shots.OnHurt(evt);

	// offscreen esp damage stuff.
	if (evt && g_menu.main.misc.log_damage_taken.get()) {
		attacker = g_csgo.m_entlist->GetClientEntity<Player*>(g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("attacker"))->GetInt()));
		victim = g_csgo.m_entlist->GetClientEntity<Player*>(g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt()));

		if (!attacker || !victim)
			return;

		if (!attacker->IsPlayer())
			return;

		player_info_t info;
		g_csgo.m_engine->GetPlayerInfo(attacker->index(), &info);


		if (attacker != g_cl.m_local && victim == g_cl.m_local) {
			std::string name{ std::string(info.m_name).substr(0, 24) };
			float damage = evt->m_keys->FindKey(HASH("dmg_health"))->GetInt();
			int hp = evt->m_keys->FindKey(HASH("health"))->GetInt();

			std::string text;
			if (hp <= 0) {
				text = "local death";
			}
			else {
				text = std::to_string(hp) + " hp left";
			}

			g_notify.add(tfm::format(XOR("got hit by %s in %s for %i damage (%i)\n"), name, g_shots.m_groups[evt->m_keys->FindKey(HASH("hitgroup"))->GetInt()], damage, text));
		}

		// a player damaged the local player.
		if (attacker->index() > 0 && attacker->index() < 64 && victim == g_cl.m_local)
			g_visuals.m_offscreen_damage[attacker->index()] = { 3.f, 0.f, colors::red };
	}

	if (evt) {
		Player* attacker = g_csgo.m_entlist->GetClientEntity<Player*>(g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("attacker"))->GetInt()));
		Player* victim = g_csgo.m_entlist->GetClientEntity<Player*>(g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt()));

		if (!attacker || !victim || attacker != g_cl.m_local)
			return;

		vec3_t enemypos = victim->GetAbsOrigin();
		impact_info best_impact;
		float best_impact_distance = -1;
		float time = g_csgo.m_globals->m_curtime;


		for (int i = 0; i < g_visuals.impacts.size(); i++) {
			auto iter = g_visuals.impacts[i];
			if (time > iter.time + 1.f) {
				g_visuals.impacts.erase(g_visuals.impacts.begin() + i);
				continue;
			}
			vec3_t position = vec3_t(iter.x, iter.y, iter.z);
			float distance = position.dist_to(enemypos);
			if (distance < best_impact_distance || best_impact_distance == -1) {
				best_impact_distance = distance;
				best_impact = iter;
			}
		}
		if (best_impact_distance == -1)
			return;

		hitmarker_info info;
		info.impact = best_impact;
		info.alpha = 255;
		info.time = g_csgo.m_globals->m_curtime;
		g_visuals.hitmarkers.push_back(info);
	}
}

void events::bullet_impact(IGameEvent* evt) {
	Player* enemy = g_csgo.m_entlist->GetClientEntity<Player*>(g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt()));
	//if (g_cl.m_processing && enemy->m_iTeamNum() != g_cl.m_local->m_iTeamNum() && g_menu.main.visuals.impact_beams.get(1) && !enemy->dormant()) {
	//	BeamInfo_t beam_info;
	//	Beam_t* beam;

	//	g_shots.OnImpact(evt);

	//	if (!g_cl.m_local || !evt)
	//		return;

	//	vec3_t pos(evt->GetFloat("x"), evt->GetFloat("y"), evt->GetFloat("z"));
	//	vec3_t start = enemy->wpn_shoot_pos();

	//	beam_info.m_vecStart = start;
	//	beam_info.m_vecEnd = pos; // if i want to invert the animations then i just have to change this to start and the start to end (and fix the jitter ofc)
	//	if (g_menu.main.visuals.impact_beams_type.get() == 0) {
	//		beam_info.m_nModelIndex = g_csgo.m_model_info->GetModelIndex(XOR("sprites/white.vmt"));
	//		beam_info.m_pszModelName = XOR("sprites/purplelaser1.vmt");
	//		beam_info.m_flHaloScale = 0.f;
	//		beam_info.m_flLife = g_menu.main.visuals.impact_beams_time.get();
	//		beam_info.m_flWidth = 2.f;
	//		beam_info.m_flEndWidth = 2.f;
	//		beam_info.m_flFadeLength = 0.f;
	//		beam_info.m_flAmplitude = 0.f;   // beam 'jitter'.
	//		beam_info.m_flBrightness = 255.f;
	//		beam_info.m_flSpeed = 0.5f;  // seems to control how fast the 'scrolling' of beam is... once fully spawned.
	//		beam_info.m_nStartFrame = 0;
	//		beam_info.m_flFrameRate = 0.f;
	//		beam_info.m_nSegments = 2;     // controls how much of the beam is 'split up', usually makes m_flAmplitude and m_flSpeed much more noticeable.
	//		beam_info.m_bRenderable = true;  // must be true or you won't see the beam.
	//		beam_info.m_nFlags = 0;
	//	}
	//	else if (g_menu.main.visuals.impact_beams_type.get() == 1) {
	//		//beam_info.m_nType = 0;
	//		beam_info.m_pszModelName = "sprites/physbeam.vmt";
	//		beam_info.m_nModelIndex = -1;
	//		beam_info.m_flHaloScale = 0.0f;
	//		beam_info.m_flLife = g_menu.main.visuals.impact_beams_time.get();

	//		beam_info.m_flFadeLength = 0.0f;
	//		beam_info.m_flAmplitude = 0.0f;
	//		beam_info.m_nSegments = 2;
	//		beam_info.m_bRenderable = true;
	//		beam_info.m_flBrightness = 255.f;
	//		beam_info.m_flSpeed = 0.2f;
	//		beam_info.m_nStartFrame = 0;
	//		beam_info.m_flFrameRate = 0.f;
	//		beam_info.m_flWidth = 2.0f;
	//		beam_info.m_flEndWidth = 2.0f;

	//		beam_info.m_nFlags = 0;
	//	}
	//	else {
	//		//beam_info.m_nType = 0;
	//		beam_info.m_pszModelName = "sprites/purplelaser1.vmt";
	//		beam_info.m_nModelIndex = g_csgo.m_model_info->GetModelIndex(XOR("sprites/purplelaser1.vmt"));
	//		beam_info.m_flHaloScale = 0.0f;
	//		beam_info.m_flLife = g_menu.main.visuals.impact_beams_time.get();

	//		beam_info.m_flFadeLength = 0.0f;
	//		beam_info.m_flAmplitude = 0.0f;
	//		beam_info.m_nSegments = 2;
	//		beam_info.m_bRenderable = true;
	//		beam_info.m_flBrightness = 180.f;
	//		beam_info.m_flSpeed = 0.2f;
	//		beam_info.m_nStartFrame = 0;
	//		beam_info.m_flFrameRate = 1.f;
	//		beam_info.m_flWidth = 6.0f;
	//		beam_info.m_flEndWidth = 6.0f;

	//		beam_info.m_nFlags = 0x8300;
	//	}

	//	beam_info.m_flRed = g_menu.main.visuals.impact_beams_enemy_color.get().r();
	//	beam_info.m_flGreen = g_menu.main.visuals.impact_beams_enemy_color.get().g();
	//	beam_info.m_flBlue = g_menu.main.visuals.impact_beams_enemy_color.get().b();


	//	// attempt to render the beam.
	//	if (g_menu.main.visuals.impact_beams_type.get() != 3) {
	//		beam = g_csgo.m_beams->CreateBeamPoints(beam_info);
	//		if (beam) {
	//			g_csgo.m_beams->DrawBeam(beam);
	//		}
	//	}
	//	// attempt to render the line.
	//	else if (g_menu.main.visuals.impact_beams_type.get() == 3) {
	//		g_csgo.m_debug_overlay->AddLineOverlay(start, pos, beam_info.m_flRed, beam_info.m_flGreen, beam_info.m_flBlue, false, g_menu.main.visuals.impact_beams_time.get());
	//	}
	//}



	if (g_cl.m_processing) {
		//auto impact = g_csgo.sv_showimpacts;

		g_shots.OnImpact(evt);

		if (!evt || !g_cl.m_local)
			return;

		int attacker = g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt());
		if (attacker != g_csgo.m_engine->GetLocalPlayer())
			return;

		Player* shooter = g_csgo.m_entlist->GetClientEntity<Player*>(g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt()));
		if (!shooter || shooter != g_cl.m_local)
			return;

		impact_info info;
		info.x = evt->GetFloat(XOR("x"));
		info.y = evt->GetFloat(XOR("y"));
		info.z = evt->GetFloat(XOR("z"));
		info.time = g_csgo.m_globals->m_curtime;
		g_visuals.m_impacts.push_back(info);

		vec3_t pos = {
				evt->m_keys->FindKey(HASH("x"))->GetFloat(),
				evt->m_keys->FindKey(HASH("y"))->GetFloat(),
				evt->m_keys->FindKey(HASH("z"))->GetFloat()
		};

		Color color = g_menu.main.visuals.server_impact.get();
		color.a() = g_menu.main.visuals.server_impact.get().a();

		// draw our (server-side) bullet impacts.
		if (g_menu.main.visuals.bullet_impacts.get())
			g_csgo.m_debug_overlay->AddBoxOverlay(vec3_t(info.x, info.y, info.z), vec3_t(-1.5, -1.5, -1.5), vec3_t(1.5, 1.5, 1.5), ang_t(0, 0, 0), color.r(), color.g(), color.b(), color.a(), 4.f);

	}
}

void events::item_purchase(IGameEvent* evt) {
	int           team, purchaser;
	player_info_t info;

	if (!g_cl.m_local || !evt)
		return;

	if (!g_menu.main.misc.log_purchases.get())
		return;

	// only log purchases of the enemy team.
	team = evt->m_keys->FindKey(HASH("team"))->GetInt();
	if (team == g_cl.m_local->m_iTeamNum())
		return;

	// get the player that did the purchase.
	purchaser = g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt());

	// get player info of purchaser.
	if (!g_csgo.m_engine->GetPlayerInfo(purchaser, &info))
		return;

	std::string weapon = evt->m_keys->FindKey(HASH("weapon"))->m_string;
	if (weapon == XOR("weapon_unknown"))
		return;

	std::string out = tfm::format(XOR("%s bought %s\n"), std::string{ info.m_name }.substr(0, 24), weapon);
	g_notify.add(out);
}

void events::player_death(IGameEvent* evt) {
	// get index of player that died.
	int index = g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt());

	// reset opacity scale.
	g_visuals.m_opacities[index - 1] = 0.f;
	g_visuals.m_draw[index - 1] = false;
}

void events::player_given_c4(IGameEvent* evt) {
	player_info_t info;


	// get the player who received the bomb.
	int index = g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt());
	if (index == g_csgo.m_engine->GetLocalPlayer())
		return;

	if (!g_csgo.m_engine->GetPlayerInfo(index, &info))
		return;

	std::string out = tfm::format(XOR("%s received the bomb\n"), std::string{ info.m_name }.substr(0, 24));
	g_notify.add(out);
}

void events::bomb_beginplant(IGameEvent* evt) {
	player_info_t info;

	// get the player who played the bomb.
	int index = g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt());
	if (index == g_csgo.m_engine->GetLocalPlayer())
		return;

	// get player info of purchaser.
	if (!g_csgo.m_engine->GetPlayerInfo(index, &info))
		return;

	std::string out = tfm::format(XOR("%s started planting the bomb\n"), std::string{ info.m_name }.substr(0, 24));
	g_notify.add(out);
}

void events::bomb_abortplant(IGameEvent* evt) {
	player_info_t info;

	// get the player who stopped planting the bomb.
	int index = g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt());
	if (index == g_csgo.m_engine->GetLocalPlayer())
		return;

	// get player info of purchaser.
	if (!g_csgo.m_engine->GetPlayerInfo(index, &info))
		return;

	std::string out = tfm::format(XOR("%s stopped planting the bomb\n"), std::string{ info.m_name }.substr(0, 24));
	g_notify.add(out);
}

void events::bomb_planted(IGameEvent* evt) {
	Entity* bomb_target;
	std::string   site_name;
	int           player_index;
	player_info_t info;
	std::string   out;

	// get the func_bomb_target entity and store info about it.
	bomb_target = g_csgo.m_entlist->GetClientEntity(evt->m_keys->FindKey(HASH("site"))->GetInt());
	if (bomb_target) {
		site_name = bomb_target->GetBombsiteName();
		g_visuals.m_last_bombsite = site_name;
	}

	player_index = g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt());
	if (player_index == g_csgo.m_engine->GetLocalPlayer())
		out = tfm::format(XOR("you planted the bomb at %s\n"), site_name.c_str());

	else {
		g_csgo.m_engine->GetPlayerInfo(player_index, &info);

		out = tfm::format(XOR("the bomb was planted at %s by %s\n"), site_name.c_str(), std::string(info.m_name).substr(0, 24));
	}

	g_notify.add(out);
}

void events::bomb_beep(IGameEvent* evt) {
	Entity* c4;
	vec3_t             explosion_origin, explosion_origin_adjusted;
	CTraceFilterSimple filter;
	CGameTrace         tr;

	// we have a bomb ent already, don't do anything else.
	if (g_visuals.m_c4_planted)
		return;

	// bomb_beep is called once when a player plants the c4 and contains the entindex of the C4 weapon itself, we must skip that here.
	c4 = g_csgo.m_entlist->GetClientEntity(evt->m_keys->FindKey(HASH("entindex"))->GetInt());
	if (!c4 || !c4->is(HASH("CPlantedC4")))
		return;

	// planted bomb is currently active, grab some extra info about it and set it for later.
	g_visuals.m_c4_planted = true;
	g_visuals.m_planted_c4 = c4;
	g_visuals.m_planted_c4_explode_time = c4->m_flC4Blow();

	// the bomb origin is adjusted slightly inside CPlantedC4::C4Think, right when it's about to explode.
	// we're going to do that here.
	explosion_origin = c4->GetAbsOrigin();
	explosion_origin_adjusted = explosion_origin;
	explosion_origin_adjusted.z += 8.f;

	// setup filter and do first trace.
	filter.SetPassEntity(c4);

	g_csgo.m_engine_trace->TraceRay(
		Ray(explosion_origin_adjusted, explosion_origin_adjusted + vec3_t(0.f, 0.f, -40.f)),
		MASK_SOLID,
		&filter,
		&tr
	);

	// pull out of the wall a bit.
	if (tr.m_fraction != 1.f)
		explosion_origin = tr.m_endpos + (tr.m_plane.m_normal * 0.6f);

	// this happens inside CCSGameRules::RadiusDamage.
	explosion_origin.z += 1.f;

	// set all other vars.
	g_visuals.m_planted_c4_explosion_origin = explosion_origin;

	// todo - dex;  get this radius dynamically... seems to only be available in map bsp file, search string: "info_map_parameters"
	//              info_map_parameters is an entity created on the server, it doesnt seem to have many useful networked vars for clients.
	//
	//              swapping maps between de_dust2 and de_nuke and scanning for 500 and 400 float values will leave you one value.
	//              need to figure out where it's written from.
	//
	// server.dll uses starting 'radius' as damage... the real radius passed to CCSGameRules::RadiusDamage is actually multiplied by 3.5.
	g_visuals.m_planted_c4_damage = 500.f;
	g_visuals.m_planted_c4_radius = g_visuals.m_planted_c4_damage * 3.5f;
	g_visuals.m_planted_c4_radius_scaled = g_visuals.m_planted_c4_radius / 3.f;
}

void events::bomb_begindefuse(IGameEvent* evt) {
	player_info_t info;

	// get index of player that started defusing the bomb.
	int index = g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt());
	if (index == g_csgo.m_engine->GetLocalPlayer())
		return;

	if (!g_csgo.m_engine->GetPlayerInfo(index, &info))
		return;

	bool kit = evt->m_keys->FindKey(HASH("haskit"))->GetBool();

	if (kit) {
		std::string out = tfm::format(XOR("%s started defusing with a kit\n"), std::string(info.m_name).substr(0, 24));
		g_notify.add(out);
	}

	else {
		std::string out = tfm::format(XOR("%s started defusing without a kit\n"), std::string(info.m_name).substr(0, 24));
		g_notify.add(out);
	}
}

void events::bomb_abortdefuse(IGameEvent* evt) {
	player_info_t info;

	// get index of player that stopped defusing the bomb.
	int index = g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt());
	if (index == g_csgo.m_engine->GetLocalPlayer())
		return;

	if (!g_csgo.m_engine->GetPlayerInfo(index, &info))
		return;

	std::string out = tfm::format(XOR("%s stopped defusing\n"), std::string(info.m_name).substr(0, 24));
	g_notify.add(out);
}

void events::bomb_exploded(IGameEvent* evt) {
	g_visuals.m_c4_planted = false;
	g_visuals.m_planted_c4 = nullptr;
	g_visuals.bombexploded = true;
}

void events::bomb_defused(IGameEvent* evt) {
	g_visuals.m_c4_planted = false;
	g_visuals.m_planted_c4 = nullptr;
	g_visuals.bombedefused = true;
}

void events::weapon_fire(IGameEvent* evt) {
	g_shots.OnWeaponFire(evt);
}

void Listener::init() {
	// link events with callbacks.
	add(XOR("round_start"), events::round_start);
	add(XOR("round_end"), events::round_end);
	add(XOR("player_hurt"), events::player_hurt);
	add(XOR("bullet_impact"), events::bullet_impact);
	add(XOR("item_purchase"), events::item_purchase);
	add(XOR("player_death"), events::player_death);
	add(XOR("player_given_c4"), events::player_given_c4);
	add(XOR("bomb_beginplant"), events::bomb_beginplant);
	add(XOR("bomb_abortplant"), events::bomb_abortplant);
	add(XOR("bomb_planted"), events::bomb_planted);
	add(XOR("bomb_beep"), events::bomb_beep);
	add(XOR("bomb_begindefuse"), events::bomb_begindefuse);
	add(XOR("bomb_abortdefuse"), events::bomb_abortdefuse);
	add(XOR("bomb_exploded"), events::bomb_exploded);
	add(XOR("bomb_defused"), events::bomb_defused);
	add(XOR("weapon_fire"), events::weapon_fire);

	register_events();
}