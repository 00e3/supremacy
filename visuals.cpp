#include "includes.h"
#include "gh.h"
#define MIN1(a,b) ((a) < (b) ? (a) : (b))

Visuals g_visuals{ };;

void Visuals::ModulateWorld() {
	std::vector< IMaterial* > world, props;
	static auto sv_skyname = g_csgo.m_cvar->FindVar(HASH("sv_skyname"));

	// iterate material handles.
	for (uint16_t h{ g_csgo.m_material_system->FirstMaterial() }; h != g_csgo.m_material_system->InvalidMaterial(); h = g_csgo.m_material_system->NextMaterial(h)) {
		// get material from handle.
		IMaterial* mat = g_csgo.m_material_system->GetMaterial(h);
		if (!mat)
			continue;

		// store world materials.
		if (FNV1a::get(mat->GetTextureGroupName()) == HASH("World textures"))
			world.push_back(mat);

		// store props.
		else if (FNV1a::get(mat->GetTextureGroupName()) == HASH("StaticProp textures"))
			props.push_back(mat);
	}

	// night

	Color col = g_menu.main.visuals.nightcolor.get();
	Color col2 = g_menu.main.visuals.propscolor.get();

	if (g_menu.main.visuals.world.get(0)) {
		for (const auto& w : world)

			w->ColorModulate(col.r() / 255.f, col.g() / 255.f, col.b() / 255.f);

		// IsUsingStaticPropDebugModes my nigga
		if (g_csgo.r_DrawSpecificStaticProp->GetInt() != 0) {
			g_csgo.r_DrawSpecificStaticProp->SetValue(0);
		}

		for (const auto& p : props)
			p->ColorModulate(col2.r() / 255.f, col2.g() / 255.f, col2.b() / 255.f);

		sv_skyname->SetValue(XOR("sky_csgo_night02"));
	}

	// disable night.
	else {
		for (const auto& w : world)
			w->ColorModulate(1.f, 1.f, 1.f);

		// restore r_DrawSpecificStaticProp.
		if (g_csgo.r_DrawSpecificStaticProp->GetInt() != -1) {
			g_csgo.r_DrawSpecificStaticProp->SetValue(-1);
		}

		for (const auto& p : props)
			p->ColorModulate(1.f, 1.f, 1.f);
	}

	float alpha = g_menu.main.visuals.transparent_props_amount.get() / 100;
	for (const auto& p : props)
		p->AlphaModulate(alpha);

	if (g_csgo.r_DrawSpecificStaticProp->GetInt() != 0) {
		g_csgo.r_DrawSpecificStaticProp->SetValue(0);
	}
}

void Visuals::ThirdpersonThink() {
	ang_t                          offset;
	vec3_t                         origin, forward;
	static CTraceFilterSimple_game filter{ };
	CGameTrace                     tr;

	// for whatever reason overrideview also gets called from the main menu.
	if (!g_csgo.m_engine->IsInGame())
		return;

	// check if we have a local player and he is alive.
	bool alive = g_cl.m_local && g_cl.m_local->alive();

	// camera should be in thirdperson.
	if (m_thirdperson) {

		// if alive and not in thirdperson already switch to thirdperson.
		if (alive && !g_csgo.m_input->CAM_IsThirdPerson())
			g_csgo.m_input->CAM_ToThirdPerson();

		// if dead and spectating in firstperson switch to thirdperson.
		else if (g_cl.m_local->m_iObserverMode() == 4) {

			// if in thirdperson, switch to firstperson.
			// we need to disable thirdperson to spectate properly.
			if (g_csgo.m_input->CAM_IsThirdPerson()) {
				g_csgo.m_input->CAM_ToFirstPerson();
				g_csgo.m_input->m_camera_offset.z = 0.f;
			}

			g_cl.m_local->m_iObserverMode() = 5;
		}
	}

	// camera should be in firstperson.
	else if (g_csgo.m_input->CAM_IsThirdPerson()) {
		g_csgo.m_input->CAM_ToFirstPerson();
		g_csgo.m_input->m_camera_offset.z = 0.f;
	}

	// if after all of this we are still in thirdperson.
	if (g_csgo.m_input->CAM_IsThirdPerson()) {
		// get camera angles.
		g_csgo.m_engine->GetViewAngles(offset);

		// get our viewangle's forward directional vector.
		math::AngleVectors(offset, &forward);

		// cam_idealdist convar.
		offset.z = g_menu.main.visuals.thirdperson_distance.get();

		// start pos.
		origin = g_cl.m_shoot_pos;

		// setup trace filter and trace.
		filter.SetPassEntity(g_cl.m_local);

		g_csgo.m_engine_trace->TraceRay(
			Ray(origin, origin - (forward * offset.z), { -16.f, -16.f, -16.f }, { 16.f, 16.f, 16.f }),
			MASK_NPCWORLDSTATIC,
			(ITraceFilter*)&filter,
			&tr
		);

		// adapt distance to travel time.
		math::clamp(tr.m_fraction, 0.f, 1.f);
		offset.z *= tr.m_fraction;

		// override camera angles.
		g_csgo.m_input->m_camera_offset = { offset.x, offset.y, offset.z };
	}
}

// meme...
//void Visuals::IndicateAngles()
//{
//	if (!g_csgo.m_engine->IsInGame() && !g_csgo.m_engine->IsConnected())
//		return;
//
//	if (!g_menu.main.antiaim.draw_angles.get())
//		return;
//
//	if (!g_csgo.m_input->CAM_IsThirdPerson())
//		return;
//
//	if (!g_cl.m_local || g_cl.m_local->m_iHealth() < 1)
//		return;
//
//	const auto& pos = g_cl.m_local->GetRenderOrigin();
//	vec2_t tmp;
//
//	if (render::WorldToScreen(pos, tmp))
//	{
//		vec2_t draw_tmp;
//		const vec3_t real_pos(50.f * std::cos(math::deg_to_rad(g_cl.m_radar.y)) + pos.x, 50.f * sin(math::deg_to_rad(g_cl.m_radar.y)) + pos.y, pos.z);
//
//		if (render::WorldToScreen(real_pos, draw_tmp))
//		{
//			render::line(tmp.x, tmp.y, draw_tmp.x, draw_tmp.y, { 0, 255, 0, 255 });
//			render::esp_small.string(draw_tmp.x, draw_tmp.y, { 0, 255, 0, 255 }, "FAKE", render::ALIGN_LEFT);
//		}
//
//		if (g_menu.main.antiaim.fake_yaw.get())
//		{
//			const vec3_t fake_pos(50.f * cos(math::deg_to_rad(g_cl.m_angle.y)) + pos.x, 50.f * sin(math::deg_to_rad(g_cl.m_angle.y)) + pos.y, pos.z);
//
//			if (render::WorldToScreen(fake_pos, draw_tmp))
//			{
//				render::line(tmp.x, tmp.y, draw_tmp.x, draw_tmp.y, { 255, 0, 0, 255 });
//				render::esp_small.string(draw_tmp.x, draw_tmp.y, { 255, 0, 0, 255 }, "REAL", render::ALIGN_LEFT);
//			}
//		}
//
//		if (g_menu.main.antiaim.body_fake_stand.get() == 1 || g_menu.main.antiaim.body_fake_stand.get() == 2 || g_menu.main.antiaim.body_fake_stand.get() == 3 || g_menu.main.antiaim.body_fake_stand.get() == 4 || g_menu.main.antiaim.body_fake_stand.get() == 5 || g_menu.main.antiaim.body_fake_stand.get() == 6)
//		{
//			float lby = g_cl.m_local->m_flLowerBodyYawTarget();
//			const vec3_t lby_pos(50.f * cos(math::deg_to_rad(lby)) + pos.x,
//				50.f * sin(math::deg_to_rad(lby)) + pos.y, pos.z);
//
//			if (render::WorldToScreen(lby_pos, draw_tmp))
//			{
//				render::line(tmp.x, tmp.y, draw_tmp.x, draw_tmp.y, { 255, 255, 255, 255 });
//				render::esp_small.string(draw_tmp.x, draw_tmp.y, { 255, 255, 255, 255 }, "LBY", render::ALIGN_LEFT);
//			}
//		}
//	}
//}

void Visuals::Hitmarker() {
	if (!g_menu.main.visuals.hitmarker.get())
		return;

	if (g_csgo.m_globals->m_curtime > m_hit_end)
		return;

	if (m_hit_duration <= 0.f)
		return;

	float complete = (g_csgo.m_globals->m_curtime - m_hit_start) / m_hit_duration;
	int x = g_cl.m_width,
		y = g_cl.m_height,
		alpha = (1.f - complete) * 240;

	constexpr int linesize = 8, linedec = 4;

	render::line(x / 2 - linesize, y / 2 - linesize, x / 2 - linedec, y / 2 - linedec, { 200, 200, 200, alpha });
	render::line(x / 2 - linesize, y / 2 + linesize, x / 2 - linedec, y / 2 + linedec, { 200, 200, 200, alpha });
	render::line(x / 2 + linesize, y / 2 + linesize, x / 2 + linedec, y / 2 + linedec, { 200, 200, 200, alpha });
	render::line(x / 2 + linesize, y / 2 - linesize, x / 2 + linedec, y / 2 - linedec, { 200, 200, 200, alpha });
}

void Visuals::NoSmoke() {
	if (!g_menu.main.visuals.removals.get(1))
		return;

	if (!smoke1)
		smoke1 = g_csgo.m_material_system->FindMaterial(XOR("particle/vistasmokev1/vistasmokev1_fire"), XOR("Other textures"));

	if (!smoke2)
		smoke2 = g_csgo.m_material_system->FindMaterial(XOR("particle/vistasmokev1/vistasmokev1_smokegrenade"), XOR("Other textures"));

	if (!smoke3)
		smoke3 = g_csgo.m_material_system->FindMaterial(XOR("particle/vistasmokev1/vistasmokev1_emods"), XOR("Other textures"));

	if (!smoke4)
		smoke4 = g_csgo.m_material_system->FindMaterial(XOR("particle/vistasmokev1/vistasmokev1_emods_impactdust"), XOR("Other textures"));

	if (g_menu.main.visuals.removals.get(1)) {
		if (!smoke1->GetFlag(MATERIAL_VAR_NO_DRAW))
			smoke1->SetFlag(MATERIAL_VAR_NO_DRAW, true);

		if (!smoke2->GetFlag(MATERIAL_VAR_NO_DRAW))
			smoke2->SetFlag(MATERIAL_VAR_NO_DRAW, true);

		if (!smoke3->GetFlag(MATERIAL_VAR_NO_DRAW))
			smoke3->SetFlag(MATERIAL_VAR_NO_DRAW, true);

		if (!smoke4->GetFlag(MATERIAL_VAR_NO_DRAW))
			smoke4->SetFlag(MATERIAL_VAR_NO_DRAW, true);
	}

	else {
		if (smoke1->GetFlag(MATERIAL_VAR_NO_DRAW))
			smoke1->SetFlag(MATERIAL_VAR_NO_DRAW, false);

		if (smoke2->GetFlag(MATERIAL_VAR_NO_DRAW))
			smoke2->SetFlag(MATERIAL_VAR_NO_DRAW, false);

		if (smoke3->GetFlag(MATERIAL_VAR_NO_DRAW))
			smoke3->SetFlag(MATERIAL_VAR_NO_DRAW, false);

		if (smoke4->GetFlag(MATERIAL_VAR_NO_DRAW))
			smoke4->SetFlag(MATERIAL_VAR_NO_DRAW, false);
	}
}

void Visuals::think() {
	// don't run anything if our local player isn't valid.
	if (!g_cl.m_local)
		return;

	if (g_menu.main.visuals.removals.get(4)
		&& g_cl.m_local->alive()
		&& g_cl.m_local->GetActiveWeapon()
		&& g_cl.m_local->GetActiveWeapon()->GetWpnData()->m_weapon_type == CSWeaponType::WEAPONTYPE_SNIPER_RIFLE
		&& g_cl.m_local->m_bIsScoped()) {

		// rebuild the original scope lines.
		int w = g_cl.m_width,
			h = g_cl.m_height,
			x = w / 2,
			y = h / 2,
			size = 1;

		// Here We Use The Euclidean distance To Get The Polar-Rectangular Conversion Formula.
		if (size > 1) {
			x -= (size / 2);
			y -= (size / 2);
		}

		// draw our lines.
		render::rect_filled(0, y, w, size, colors::black);
		render::rect_filled(x, 0, size, h, colors::black);
	}

	// draw esp on ents.
	for (int i{ 1 }; i <= g_csgo.m_entlist->GetHighestEntityIndex(); ++i) {
		Entity* ent = g_csgo.m_entlist->GetClientEntity(i);
		if (!ent)
			continue;

		draw(ent);
	}

	// draw everything else.
	//SpreadCrosshair();
	StatusIndicators();
	Spectators();
	PenetrationCrosshair();
	ManualAntiAim();
	Hitmarker();
	//Hitmarker3D();
	DrawPlantedC4();

	//nade prediction stuff
	auto& predicted_nades = g_grenades_pred.get_list();

	static auto last_server_tick = g_csgo.m_cl->m_server_tick;
	if (g_csgo.m_cl->m_server_tick != last_server_tick) {
		predicted_nades.clear();

		last_server_tick = g_csgo.m_cl->m_server_tick;
	}

	// draw esp on ents.
	for (int i{ 1 }; i <= g_csgo.m_entlist->GetHighestEntityIndex(); ++i) {
		Entity* ent = g_csgo.m_entlist->GetClientEntity(i);
		if (!ent)
			continue;

		if (ent->dormant())
			continue;

		if (!ent->is(HASH("CMolotovProjectile"))
			&& !ent->is(HASH("CBaseCSGrenadeProjectile")))
			continue;

		if (ent->is(HASH("CBaseCSGrenadeProjectile"))) {
			const auto studio_model = ent->GetModel();
			if (!studio_model
				|| std::string_view(studio_model->m_name).find("fraggrenade") == std::string::npos)
				continue;
		}

		const auto handle = reinterpret_cast<Player*>(ent)->GetRefEHandle();

		if (ent->m_fEffects() & EF_NODRAW) {
			predicted_nades.erase(handle);

			continue;
		}

		if (predicted_nades.find(handle) == predicted_nades.end()) {
			predicted_nades.emplace(
				std::piecewise_construct,
				std::forward_as_tuple(handle),
				std::forward_as_tuple(
					reinterpret_cast<Player*>(g_csgo.m_entlist->GetClientEntityFromHandle(ent->m_hThrower())),
					ent->is(HASH("CMolotovProjectile")) ? MOLOTOV : HEGRENADE,
					ent->m_vecOrigin(), ent->m_vecVelocity(), ent->m_flSpawnTime_Grenade(),
					game::TIME_TO_TICKS(reinterpret_cast<Player*>(ent)->m_flSimulationTime() - ent->m_flSpawnTime_Grenade())
				)
			);
		}

		if (predicted_nades.at(handle).draw())
			continue;

		predicted_nades.erase(handle);
	}

	g_grenades_pred.get_local_data().draw();
}

void Visuals::Spectators() {


	std::vector< std::string > spectators{ XOR("") };
	int h = render::menu_shade.m_size.m_height;

	for (int i{ 1 }; i <= g_csgo.m_globals->m_max_clients; ++i) {
		Player* player = g_csgo.m_entlist->GetClientEntity< Player* >(i);
		if (!player)
			continue;

		if (player->m_bIsLocalPlayer())
			continue;

		if (player->dormant())
			continue;

		if (player->m_lifeState() == LIFE_ALIVE)
			continue;

		if (player->GetObserverTarget() != g_cl.m_local)
			continue;

		player_info_t info;
		if (!g_csgo.m_engine->GetPlayerInfo(i, &info))
			continue;

		spectators.push_back(std::string(info.m_name).substr(0, 24));
	}

	size_t total_size = spectators.size() * (h - 1);

	for (size_t i{ }; i < spectators.size(); ++i) {
		const std::string& name = spectators[i];

		render::menu_shade.string(g_cl.m_width - 4, (i * 14) - 10, { 255, 255, 255, 179 }, name, render::ALIGN_RIGHT);
	}
}

void Visuals::StatusIndicators() {
	auto local_player = g_cl.m_local;

	if (!g_cl.m_processing)
		return;

	struct Indicator_t { Color color; std::string text; };
	std::vector< Indicator_t > indicators{ };

	// LBY
	if (g_menu.main.visuals.indicators.get(0)) {
		// get the absolute change between current lby and animated angle.
		float change = std::abs(math::NormalizedAngle(g_cl.m_body - g_cl.m_angle.y));

		Indicator_t ind{ };
		ind.color = change > 35.f ? 0xff15c27b : 0xff0000ff;
		ind.text = XOR("LBY");
		indicators.push_back(ind);
	}

	// LC
	if (g_menu.main.visuals.indicators.get(1) && ((g_cl.m_buttons & IN_JUMP) || !(g_cl.m_flags & FL_ONGROUND))) {
		Indicator_t ind{ };
		ind.color = g_cl.m_lagcomp ? 0xff15c27b : 0xff0000ff;
		ind.text = XOR("LC");

		indicators.push_back(ind);
	}

	// PING
	if (g_menu.main.visuals.indicators.get(2) && g_menu.main.misc.ping_spike.get()) {
		Indicator_t ind{};
		Color Ping_Transition = Color(255, 255, 255, 210);
		Color Ting_Pransition = 0xff15c27b;
		float PatrickDog = 0.012f;

		static float transitionProgress = 0.0f;

		if (g_aimbot.m_fake_latency2) {
			transitionProgress += PatrickDog;
			if (transitionProgress > 1.0f)
				transitionProgress = 1.0f;
		}
		else {
			transitionProgress -= PatrickDog;
			if (transitionProgress < 0.0f)
				transitionProgress = 0.0f;
		}

		ind.color = Color(
			Ping_Transition.r() + std::round((Ting_Pransition.r() - Ping_Transition.r()) * transitionProgress),
			Ping_Transition.g() + std::round((Ting_Pransition.g() - Ping_Transition.g()) * transitionProgress),
			Ping_Transition.b() + std::round((Ting_Pransition.b() - Ping_Transition.b()) * transitionProgress),
			Ping_Transition.a() + std::round((Ting_Pransition.a() - Ping_Transition.a()) * transitionProgress)
		);

		ind.text = XOR("PING");
		indicators.push_back(ind);
	}

	// PING
	if (g_menu.main.visuals.indicators.get(3) && g_aimbot.m_damage_toggle) {
		Indicator_t ind{ };
		ind.color = 0xffffffff;
		ind.text = XOR("DMG"); //std::to_string(int(g_menu.main.aimbot.override_dmg_value.get()));

		indicators.push_back(ind);
	}

	if (indicators.empty())
		return;

	// iterate and draw indicators.
	for (size_t i{ }; i < indicators.size(); ++i) {
		auto& indicator = indicators[i];

		render::indicator.string(20, g_cl.m_height - 80 - (30 * i), indicator.color, indicator.text);
	}
}

/*void Visuals::SpreadCrosshair() {
	// dont do if dead.
	if (!g_cl.m_processing)
		return;

	if (!g_menu.main.visuals.spread_xhair.get())
		return;

	// get active weapon.
	Weapon* weapon = g_cl.m_local->GetActiveWeapon();
	if (!weapon)
		return;

	WeaponInfo* data = weapon->GetWpnData();
	if (!data)
		return;

	// do not do this on: bomb, knife and nades.
	int type = weapon->m_iItemDefinitionIndex();
	if (type == WEAPONTYPE_KNIFE || type == WEAPONTYPE_C4 || type == WEAPONTYPE_GRENADE)
		return;

	// calc radius.
	float radius = ((weapon->GetInaccuracy() + weapon->GetSpread()) * 320.f) / (std::tan(math::deg_to_rad(g_cl.m_local->GetFOV()) * 0.5f) + FLT_EPSILON);

	// scale by screen size.
	radius *= g_cl.m_height * (1.f / 480.f);

	// get color.
	Color col = g_menu.main.visuals.spread_xhair_col.get();

	// modify alpha channel.
	col.a() = 200 * (g_menu.main.visuals.spread_xhair_blend.get() / 100.f);

	int segements = std::max(16, (int)std::round(radius * 0.75f));
	render::circle(g_cl.m_width / 2, g_cl.m_height / 2, radius, segements, col);
}*/

//void Visuals::SpreadCrosshair() {
//	// dont do if dead.
//	if (!g_cl.m_processing)
//		return;
//
//	if (!g_menu.main.visuals.spread_xhair.get())
//		return;
//
//	// get active weapon.
//	Weapon* weapon = g_cl.m_local->GetActiveWeapon();
//	if (!weapon)
//		return;
//
//	WeaponInfo* data = weapon->GetWpnData();
//	if (!data)
//		return;
//
//	// do not do this on: bomb, knife and nades.
//	int type = weapon->m_iItemDefinitionIndex();
//	if (type == WEAPONTYPE_KNIFE || type == WEAPONTYPE_C4 || type == WEAPONTYPE_GRENADE)
//		return;
//
//	int w, h;
//	g_csgo.m_engine->GetScreenSize(w, h);
//
//	float spreadDist = ((weapon->GetInaccuracy() + weapon->GetSpread()) * 320.f) / std::tan(math::deg_to_rad(g_cl.m_local->GetFOV()) * 0.5f);
//	float spreadRadius = (spreadDist * (h / 480.f)) * 50 / 250.f;
//
//
//	for (float i = 0; i <= spreadRadius; i++)
//	{
//		Color col = g_menu.main.visuals.spread_xhair_col.get();
//		col.a() = (static_cast<int>(i * (255.f / spreadRadius)) * g_menu.main.visuals.spread_xhair_blend.get() / 100.f);
//		g_csgo.m_surface->DrawSetColor(col);
//		g_csgo.m_surface->DrawOutlinedCircle(w / 2, h / 2, static_cast<int>(i), 240);
//	}
//}

void RenderPolygon(float x, float y, float size, float rotation, Color color) {
	Vertex verts[3], verts2[3];


	verts[0] = { x, y };        // 0,  0
	verts[1] = { x, y + (18.f * size / 8.f) }; // -1, 1
	verts[2] = { x + (14.f * size / 8.f), y + (24.f * size / 8.f) }; // 1,  1

	verts2[0] = { x, y };        // 0,  0
	verts2[1] = { x, y + (18.f * size / 8.f) }; // -1, 1
	verts2[2] = { x - (14.f * size / 8.f), y + (24.f * size / 8.f) }; // 1,  1

	verts[0] = render::RotateVertex(vec2_t(x, y), verts[0], rotation * 90);
	verts[1] = render::RotateVertex(vec2_t(x, y), verts[1], rotation * 90);
	verts[2] = render::RotateVertex(vec2_t(x, y), verts[2], rotation * 90);

	verts2[0] = render::RotateVertex(vec2_t(x, y), verts2[0], rotation * 90);
	verts2[1] = render::RotateVertex(vec2_t(x, y), verts2[1], rotation * 90);
	verts2[2] = render::RotateVertex(vec2_t(x, y), verts2[2], rotation * 90);

	g_csgo.m_surface->DrawSetColor(color);
	g_csgo.m_surface->DrawTexturedPolygon(3, verts);
	g_csgo.m_surface->DrawTexturedPolygon(3, verts2);

}

void Visuals::ManualAntiAim() {
	int   x, y;

	// dont do if dead.
	if (!g_cl.m_processing)
		return;

	if (!g_menu.main.antiaim.manul_antiaim.get())
		return;

	x = g_cl.m_width / 2;
	y = g_cl.m_height / 2;

	Color color = g_menu.main.antiaim.color_manul_antiaim.get();
	color.a() = g_menu.main.antiaim.color_manul_antiaim.get().a();


	if (g_hvh.m_left)
		RenderPolygon(x - 60, y, 5.f, 3, color);

	if (g_hvh.m_right)
		RenderPolygon(x + 60, y, 5.f, 1, color);

	if (g_hvh.m_back)
		RenderPolygon(x, y + 60, 5.f, 2, color);

	if (g_hvh.m_front)
		RenderPolygon(x, y - 60, 5.f, 4, color);
}

void Visuals::PenetrationCrosshair() {
	int   x, y;
	bool  valid_player_hit;
	Color final_color;

	if (!g_menu.main.visuals.pen_crosshair.get() || !g_cl.m_processing)
		return;

	x = g_cl.m_width / 2;
	y = g_cl.m_height / 2;

	valid_player_hit = (g_cl.m_pen_data.m_target && g_cl.m_pen_data.m_target->enemy(g_cl.m_local));
	if (valid_player_hit)
		final_color = colors::light_blue;

	else if (g_cl.m_pen_data.m_pen)
		final_color = colors::transparent_green;

	else
		final_color = colors::transparent_red;

	// draw small square in center of screen.
	int damage1337 = g_cl.m_pen_data.m_damage;


	render::rect_filled(x - 1, y - 1, 3, 3, final_color);
	
}

void Visuals::draw(Entity* ent) {
	if (ent->IsPlayer()) {
		Player* player = ent->as< Player* >();

		// dont draw dead players.
		if (!player->alive())
			return;

		if (player->m_bIsLocalPlayer())
			return;

		// draw player esp.
		DrawPlayer(player);
	}

	else if (g_menu.main.visuals.items_adds.get(0) && g_menu.main.visuals.items.get() && ent->IsBaseCombatWeapon() && !ent->dormant())
		DrawItem(ent->as< Weapon* >());

	else if (g_menu.main.visuals.proj.get() && g_menu.main.visuals.proj_adds.get(0))
		DrawProjectile(ent->as< Weapon* >());
}

void Visuals::DrawProjectile(Weapon* ent) {
	vec2_t screen;
	vec3_t origin = ent->GetAbsOrigin();
	if (!render::WorldToScreen(origin, screen))
		return;

	Color col = g_menu.main.visuals.proj_color.get();
	col.a() = g_menu.main.visuals.proj_color.get().a();

	// draw decoy.
	if (ent->is(HASH("CDecoyProjectile")))
		render::esp_small.string(screen.x, screen.y, col, XOR("DECOY"), render::ALIGN_CENTER);

	// draw molotov.
	else if (ent->is(HASH("CMolotovProjectile")))
		render::esp_small.string(screen.x, screen.y, col, XOR("MOLLY"), render::ALIGN_CENTER);

	else if (ent->is(HASH("CBaseCSGrenadeProjectile"))) {
		const model_t* model = ent->GetModel();

		if (model) {
			// grab modelname.
			std::string name{ ent->GetModel()->m_name };

			if (name.find(XOR("flashbang")) != std::string::npos)
				render::esp_small.string(screen.x, screen.y, col, XOR("FLASH"), render::ALIGN_CENTER);

			else if (name.find(XOR("fraggrenade")) != std::string::npos) {
				render::esp_small.string(screen.x, screen.y, col, XOR("FRAG"), render::ALIGN_CENTER);
			}
		}
	}

	// find classes.
	else if (ent->is(HASH("CInferno"))) {
		render::esp_small.string(screen.x, screen.y, col, XOR("MOLLY"), render::ALIGN_CENTER);
	}

	else if (ent->is(HASH("CSmokeGrenadeProjectile")))
		render::esp_small.string(screen.x, screen.y, col, XOR("SMOKE"), render::ALIGN_CENTER);
}

void Visuals::DrawItem(Weapon* item) {
	// we only want to draw shit without owner.
	Entity* owner = g_csgo.m_entlist->GetClientEntityFromHandle(item->m_hOwnerEntity());
	if (owner)
		return;

	std::string distance;
	int dist = (((item->m_vecOrigin() - g_cl.m_local->m_vecOrigin()).length_sqr()) * 0.0625) * 0.001;
	//if (dist > 0)
	//distance = tfm::format(XOR("%i FT"), dist);
	if (dist > 0) {
		if (dist > 5) {
			while (!(dist % 5 == 0)) {
				dist = dist - 1;
			}

			if (dist % 5 == 0)
				distance = tfm::format(XOR("%i FT"), dist);
		}
		else
			distance = tfm::format(XOR("%i FT"), dist);
	}

	// is the fucker even on the screen?
	vec2_t screen;
	vec3_t origin = item->GetAbsOrigin();
	if (!render::WorldToScreen(origin, screen))
		return;

	WeaponInfo* data = item->GetWpnData();
	if (!data)
		return;

	Color col = g_menu.main.visuals.item_color.get();
	col.a() = g_menu.main.visuals.item_color.get().a();

	//Color col1337 = g_menu.main.visuals.dropammo_color.get();
	//col1337.a() = 0xb4;

	// render bomb in green.
	if (item->is(HASH("CC4")))

		render::esp_small.string(screen.x, screen.y, { 150, 200, 60, 0xb4 }, XOR("BOMB"), render::ALIGN_CENTER);

	// if not bomb
	// normal item, get its name.
	else {
		std::string name{ item->GetLocalizedName() };

		// smallfonts needs uppercase.
		std::transform(name.begin(), name.end(), name.begin(), ::toupper);

		if (g_menu.main.visuals.distance.get())
			render::esp_small.string(screen.x, screen.y - 8, col, distance, render::ALIGN_CENTER);
		render::esp_small.string(screen.x, screen.y, col, name, render::ALIGN_CENTER);
	}

	//if (!g_menu.main.visuals.ammo.get())
	//	return;

	//// nades do not have ammo.
	//if (data->m_weapon_type == WEAPONTYPE_GRENADE || data->m_weapon_type == WEAPONTYPE_KNIFE)
	//	return;

	//if (item->m_iItemDefinitionIndex() == 0 || item->m_iItemDefinitionIndex() == C4)
	//	return;

	//std::string ammo = tfm::format(XOR("(%i/%i)"), item->m_iClip1(), item->m_iPrimaryReserveAmmoCount());
	////render::esp_small.string( screen.x, screen.y - render::esp_small.m_size.m_height - 1, col, ammo, render::ALIGN_CENTER );

	//int current = item->m_iClip1();
	//int max = data->m_max_clip1;
	//float scale = (float)current / max;
	//int bar = (int)std::round((51 - 2) * scale);
	//render::rect_filled(screen.x - 25, screen.y + 9, 51, 4, { 0,0,0,180 });
	//render::rect_filled(screen.x - 25 + 1, screen.y + 9 + 1, bar, 2, col1337);

	/*if (g_csgo.m_entlist->GetClientEntity(83)) {
		/*vec2_t screen;
		vec3_t origin = g_csgo.m_entlist->GetClientEntity(83)->GetAbsOrigin();
		if (!render::WorldToScreen(origin, screen))
			return;

		vec2_t screen;
		vec3_t origin = g_csgo.m_entlist->GetClientEntity(83)->GetAbsOrigin();
		if (!render::WorldToScreen(origin, screen))
			return;

		render::esp_small.string(screen.x, screen.y, colors::light_blue, XOR("TEST"), render::ALIGN_CENTER);
	}*/
}

void Visuals::OffScreen(Player* player, int alpha) {
	vec3_t view_origin, target_pos, delta;
	vec2_t screen_pos, offscreen_pos;
	float  leeway_x, leeway_y, radius, offscreen_rotation;
	bool   is_on_screen;
	Vertex verts[3], verts_outline[3];
	Color  color;

	// todo - dex; move this?
	static auto get_offscreen_data = [](const vec3_t& delta, float radius, vec2_t& out_offscreen_pos, float& out_rotation) {
		ang_t  view_angles(g_csgo.m_view_render->m_view.m_angles);
		vec3_t fwd, right, up(0.f, 0.f, 1.f);
		float  front, side, yaw_rad, sa, ca;

		// get viewport angles forward directional vector.
		math::AngleVectors(view_angles, &fwd);

		// convert viewangles forward directional vector to a unit vector.
		fwd.z = 0.f;
		fwd.normalize();

		// calculate front / side positions.
		right = up.cross(fwd);
		front = delta.dot(fwd);
		side = delta.dot(right);

		// setup offscreen position.
		out_offscreen_pos.x = radius * -side;
		out_offscreen_pos.y = radius * -front;

		// get the rotation ( yaw, 0 - 360 ).
		out_rotation = math::rad_to_deg(std::atan2(out_offscreen_pos.x, out_offscreen_pos.y) + math::pi);

		// get needed sine / cosine values.
		yaw_rad = math::deg_to_rad(-out_rotation);
		sa = std::sin(yaw_rad);
		ca = std::cos(yaw_rad);

		// rotate offscreen position around.
		out_offscreen_pos.x = (int)((g_cl.m_width / 2.f) + (radius * sa));
		out_offscreen_pos.y = (int)((g_cl.m_height / 2.f) - (radius * ca));
	};

	if (!g_menu.main.players.out_of_pov.get())
		return;

	if (!g_cl.m_processing || !g_cl.m_local->enemy(player))
		return;

	// get the player's center screen position.
	target_pos = player->WorldSpaceCenter();
	is_on_screen = render::WorldToScreen(target_pos, screen_pos);

	// give some extra room for screen position to be off screen.
	leeway_x = g_cl.m_width / 18.f;
	leeway_y = g_cl.m_height / 18.f;

	// origin is not on the screen at all, get offscreen position data and start rendering.
	if (!is_on_screen
		|| screen_pos.x < -leeway_x
		|| screen_pos.x >(g_cl.m_width + leeway_x)
		|| screen_pos.y < -leeway_y
		|| screen_pos.y >(g_cl.m_height + leeway_y)) {

		float size = g_menu.main.players.offscreen_mode.get() / 100.f;
		float pos = g_menu.main.players.offscreen_mode1.get();

		// get viewport origin.
		view_origin = g_csgo.m_view_render->m_view.m_origin;

		// get direction to target.
		delta = (target_pos - view_origin).normalized();

		// note - dex; this is the 'YRES' macro from the source sdk.
		radius = pos * (g_cl.m_height / 480.f);

		// get the data we need for rendering.
		get_offscreen_data(delta, radius, offscreen_pos, offscreen_rotation);

		// bring rotation back into range... before rotating verts, sine and cosine needs this value inverted.
		// note - dex; reference: 
		// https://github.com/VSES/SourceEngine2007/blob/43a5c90a5ada1e69ca044595383be67f40b33c61/src_main/game/client/tf/tf_hud_damageindicator.cpp#L182
		offscreen_rotation = -offscreen_rotation;

		// setup vertices for the triangle.
		verts[0] = { offscreen_pos.x + (1 * size) , offscreen_pos.y + (1 * size) };        // 0,  0
		verts[1] = { offscreen_pos.x - (12.f * size), offscreen_pos.y + (24.f * size) }; // -1, 1
		verts[2] = { offscreen_pos.x + (12.f * size), offscreen_pos.y + (24.f * size) }; // 1,  1

		// setup verts for the triangle's outline.
		verts_outline[0] = { verts[0].m_pos.x - 1.f, verts[0].m_pos.y - 1.f };
		verts_outline[1] = { verts[1].m_pos.x - 1.f, verts[1].m_pos.y + 1.f };
		verts_outline[2] = { verts[2].m_pos.x + 1.f, verts[2].m_pos.y + 1.f };

		// rotate all vertices to point towards our target.
		verts[0] = render::RotateVertex(offscreen_pos, verts[0], offscreen_rotation);
		verts[1] = render::RotateVertex(offscreen_pos, verts[1], offscreen_rotation);
		verts[2] = render::RotateVertex(offscreen_pos, verts[2], offscreen_rotation);

		// render!
		int alpha1337 = sin(abs(fmod(-math::pi + (g_csgo.m_globals->m_curtime * (2 / .75)), (math::pi * 2)))) * 255;

		if (alpha1337 < 0)
			alpha1337 = alpha1337 * (-1);

		color = g_menu.main.players.out_of_pov_color.get(); // damage_data.m_color;
		color.a() = (alpha == 255) ? alpha1337 : alpha / 2;

		g_csgo.m_surface->DrawSetColor(color);
		g_csgo.m_surface->DrawTexturedPolygon(3, verts);

	}
}

void Visuals::ImpactData() {
	if (!g_cl.m_processing) return;

	if (!g_menu.main.visuals.bullet_impacts.get()) return;

	static auto last_count = 0;
	auto& client_impact_list = *(CUtlVector< client_hit_verify_t >*)((uintptr_t)g_cl.m_local + 0xBA84);

	Color color = g_menu.main.visuals.client_impact.get();
	color.a() = g_menu.main.visuals.client_impact.get().a();

	for (auto i = client_impact_list.Count(); i > last_count; i--)
	{
		g_csgo.m_debug_overlay->AddBoxOverlay(client_impact_list[i - 1].pos, vec3_t(-1.5, -1.5, -1.5), vec3_t(1.5, 1.5, 1.5), ang_t(0, 0, 0), color.r(), color.g(), color.b(), color.a(), 4.f);
	}

	if (client_impact_list.Count() != last_count)
		last_count = client_impact_list.Count();
}

Color determine_clr(Player* player, Color clr, float alpha)
{
	if (player->dormant())
		return Color(clr.r(), clr.g(), clr.b(), alpha);
	else {
		return clr.malpha(alpha);
	}
}

std::string Visuals::GetWeaponIcon(const int id) {
	auto search = m_weapon_icons.find(id);
	if (search != m_weapon_icons.end())
		return std::string(&search->second, 1);

	return XOR("");
}


void Visuals::DrawPlayer(Player* player) {
	constexpr float MAX_DORMANT_TIME = 10.f;
	constexpr float DORMANT_FADE_TIME = MAX_DORMANT_TIME / 2.f;

	Rect		  box;
	player_info_t info;
	Color		  color;

	std::string distance;
	int dist = (((player->m_vecOrigin() - g_cl.m_local->m_vecOrigin()).length_sqr()) * 0.0625) * 0.001;
	int  distance1337{ 0 };
	if (dist < 0)
		distance1337 = 0;

	if (dist > 0) {
		distance1337 = 9 + 0;
		if (dist > 5) {
			while (!(dist % 5 == 0)) {
				dist = dist - 1;
			}

			if (dist % 5 == 0)
				distance = tfm::format(XOR("%i FT"), dist);
		}
		else
			distance = tfm::format(XOR("%i FT"), dist);
	}

	// get player index.
	int index = player->index();

	// get reference to array variable.
	float& opacity = m_opacities[index - 1];
	bool& draw = m_draw[index - 1];

	// opacity should reach 1 in 300 milliseconds.
	constexpr int frequency = 1.f / 0.3f;

	// the increment / decrement per frame.
	float step = frequency * g_csgo.m_globals->m_frametime;

	// is player enemy.
	bool enemy = player->enemy(g_cl.m_local);
	bool dormant = player->dormant();

	if (g_menu.main.visuals.enemy_radar.get() && enemy && !dormant)
		player->m_bSpotted() = true;

	// we can draw this player again.
	if (!dormant)
		draw = true;

	if (!draw)
		return;

	// if non-dormant	-> increment
	// if dormant		-> decrement
	dormant ? opacity -= step : opacity += step;

	// is dormant esp enabled for this player.
	bool dormant_esp = enemy && g_menu.main.players.dormant.get();

	// clamp the opacity.
	math::clamp(opacity, 0.f, 1.f);
	if (!opacity && !dormant_esp)
		return;

	// stay for x seconds max.
	float dt = g_csgo.m_globals->m_curtime - player->m_flSimulationTime();
	if (dormant && dt > MAX_DORMANT_TIME)
		return;

	// calculate alpha channels.
	int alpha = (int)(255.f * opacity);
	int low_alpha = (int)(179.f * opacity);

	// get color based on enemy or not.
	color = enemy ? g_menu.main.players.box_color.get() : g_menu.main.players.box_color.get();

	if (dormant && dormant_esp) {
		alpha = 112;
		low_alpha = 80;

		// fade.
		if (dt > DORMANT_FADE_TIME) {
			// for how long have we been fading?
			float faded = (dt - DORMANT_FADE_TIME);
			float scale = 1.f - (faded / DORMANT_FADE_TIME);

			alpha *= scale;
			low_alpha *= scale;
		}

		// override color.
		color = { 112, 112, 112 };
	}

	// override alpha.
	color.a() = alpha;

	// get player info.
	if (!g_csgo.m_engine->GetPlayerInfo(index, &info))
		return;

	// run offscreen ESP.
	OffScreen(player, alpha);

	// attempt to get player box.
	if (!GetPlayerBoxRect(player, box)) {
		// OffScreen( player );
		return;
	}


	bool bone_esp = (enemy && g_menu.main.players.skeleton.get()) || (!enemy && g_menu.main.players.teammates.get());
	if (bone_esp)
		DrawSkeleton(player, alpha);

	bool bone_esp2 = (enemy && g_menu.main.players.skeleton_history.get()) || (!enemy && g_menu.main.players.teammates.get());
	if (bone_esp2)
		DrawHistorySkeleton(player, alpha);

	// is box esp enabled for this player.
	bool box_esp = (enemy && g_menu.main.players.box.get()) || (!enemy && g_menu.main.players.teammates.get());

	// render box if specified.
	if (box_esp)
		render::rect_outlined(box.x, box.y, box.w, box.h, determine_clr(player, Color(color.r(), color.g(), color.b(), g_menu.main.players.box_color.get().a()), alpha).malpha(g_menu.main.players.box_color.get().a()), determine_clr(player, Color(0, 0, 0, g_menu.main.players.box_color.get().a()), low_alpha).malpha(g_menu.main.players.box_color.get().a()));
	// is name esp enabled for this player.
	bool name_esp = (enemy && g_menu.main.players.name.get()) || (!enemy && g_menu.main.players.teammates.get());

	// draw name.
	if (name_esp) {
		// fix retards with their namechange meme 
		// the point of this is overflowing unicode compares with hardcoded buffers, good hvh strat
		std::string name{ std::string(info.m_name).substr(0, 24) };

		Color clr = g_menu.main.players.name_color.get();
		render::esp.string(box.x + (box.w / 2), box.y - render::esp.m_size.m_height, determine_clr(player, clr, low_alpha).malpha(g_menu.main.players.name_color.get().a()), name, render::ALIGN_CENTER);
	}

	// is health esp enabled for this player.
	bool health_esp = (enemy && g_menu.main.players.health.get()) || (!enemy && g_menu.main.players.teammates.get());

	if (health_esp) {
		int y = box.y + 1;
		int h = box.h - 2;

		// retarded servers that go above 100 hp..
		int hp = std::min(100, player->m_iHealth());

		// calculate hp bar color.
		int r = std::min((510 * (100 - hp)) / 100, 255);
		int g = std::min((510 * hp) / 100, 255);

		// get hp bar height.
		int fill = (int)std::round(hp * h / 100.f);

		// render background.
		render::rect_filled(box.x - 6, box.y - 1, 4, box.h + 2, { 0, 0, 0, low_alpha });

		// render actual bar.
		render::rect(box.x - 5, box.y + (box.h - fill) - 2, 2, fill + 2, { r,g, 0, alpha });

		// if hp is below max, draw a string.
		if (player->m_iHealth() <= 92)
			render::esp_small.string(box.x - 5, box.y + (box.h - fill) - 7, { 255, 255, 255, low_alpha }, std::to_string(hp), render::ALIGN_CENTER);
	}

	// draw flags.
	{
		std::vector< std::pair< std::string, Color > > flags;

		auto items = enemy ? g_menu.main.players.flags.GetActiveIndices() : g_menu.main.players.flag2s.GetActiveIndices();

		// NOTE FROM NITRO TO DEX -> stop removing my iterator loops, i do it so i dont have to check the size of the vector
		// with range loops u do that to do that.
		for (auto it = items.begin(); it != items.end(); ++it) {

			// money.
			if (*it == 0)
				flags.push_back({ tfm::format(XOR("$%i"), player->m_iAccount()), { 155, 210, 100, low_alpha } });

			// armor.
			if (*it == 1) {
				if (player->m_ArmorValue() > 0) {
					if (player->m_bHasHelmet())
						flags.push_back({ XOR("HK"), {  255, 255, 255, low_alpha } });
					else
						flags.push_back({ XOR("K"), {  255, 255, 255, low_alpha } });
				}
			}

			// scoped.
			if (*it == 2 && player->m_bIsScoped())
				flags.push_back({ XOR("ZOOM"), { 60, 180, 225, low_alpha } });

			// flashed.
			if (*it == 3 && player->m_flFlashBangTime() > 0.f)
				flags.push_back({ XOR("FLASHED"), { 255, 255, 0, low_alpha } });

			// reload.
			if (*it == 4) {
				// get ptr to layer 1.
				C_AnimationLayer* layer1 = &player->m_AnimOverlay()[1];

				// check if reload animation is going on.
				if (layer1->m_weight != 0.f && player->GetSequenceActivity(layer1->m_sequence) == 967 /* ACT_CSGO_RELOAD */)
					flags.push_back({ XOR("RELOAD"), { 60, 180, 225, low_alpha } });
			}

			// bomb.
			if (*it == 5 && player->HasC4())
				flags.push_back({ XOR("BOMB"), { 255, 0, 0, low_alpha } });

			AimPlayer* data = &g_aimbot.m_players[player->index() - 1];


			if (*it == 6 && data && data->m_records.size() > 0) {
				LagRecord* current = data->m_records.front().get();
				Color broke_lc = current->m_broke_lc ? Color(155, 210, 100, low_alpha) : Color(255, 0, 0, low_alpha);
				bool extrap = current->m_broke_lc ? true : false;


				if (!(current->m_pred_flags & FL_ONGROUND))
					flags.push_back({ XOR("LC"), broke_lc });

				if (g_menu.main.aimbot.extras.get(0)) {
					if (extrap && !(current->m_pred_flags & FL_ONGROUND))
						flags.push_back({ XOR("EP"), { 255,255,255, low_alpha } });
				}
			}

			if (*it == 7) {
				if (g_cl.m_resource != nullptr) {
					auto player_resource = *(g_cl.m_resource);
					int ping = round(player_resource->GetPlayerPing(player->index()));

					//Color ping_flag;
					//if (ping <= 70) {
					//	ping_flag = Color(150, 200, 60, low_alpha_text);
					//}
					//else if (ping <= 300) {
					//	ping_flag = Color(225, 137, 49, low_alpha_text);
					//}
					//else {
					//	ping_flag = Color(255, 0, 0, low_alpha_text);
					//}


					float ping_percent = std::clamp(ping, 0, 750) / 750.f;

					Color start_color(157, 219, 90, low_alpha);
					Color middle_color(242, 151, 51, low_alpha);
					Color end_color(242, 51, 51, low_alpha);

					int r, g, b;
					if (ping <= 150) {
						// Green color
						r = start_color.r();
						g = start_color.g();
						b = start_color.b();
					}
					else if (ping <= 350) {
						float t = (ping - 150) / 200.0f;
						r = start_color.r() + static_cast<int>((middle_color.r() - start_color.r()) * t);
						g = start_color.g() + static_cast<int>((middle_color.g() - start_color.g()) * t);
						b = start_color.b() + static_cast<int>((middle_color.b() - start_color.b()) * t);
					}
					else {
						float t = (ping - 350) / 250.0f;
						r = middle_color.r() + static_cast<int>((end_color.r() - middle_color.r()) * t);
						g = middle_color.g() + static_cast<int>((end_color.g() - middle_color.g()) * t);
						b = middle_color.b() + static_cast<int>((end_color.b() - middle_color.b()) * t);
					}

					Color ping_flag(r, g, b, low_alpha);

					flags.push_back({ tfm::format(XOR("%i ms"), ping), ping_flag });
					//flags.push_back({ tfm::format(XOR("PING")), ping_flag });
				}
			}
			if (*it == 8 && !dormant) {
				auto m_weapon = g_cl.m_local->GetActiveWeapon();

				if (g_cl.m_weapon_id == SSG08 && player->m_iHealth() <= 93) {
					flags.push_back({ XOR("LETHAL"), { 150, 200, 60, low_alpha } });
				}
				else if (g_cl.m_weapon_id == AWP && player->m_iHealth() <= 100) {
					flags.push_back({ XOR("LETHAL"), { 0, 200, 0, low_alpha } });
				}
				else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20 && player->m_iHealth() <= 40) {
					flags.push_back({ XOR("LETHAL"), { 150, 200, 60, low_alpha } });
				}
				else if (g_cl.m_weapon_id == G3SG1 || g_cl.m_weapon_id == SCAR20 && player->m_iHealth() <= 81) {
					flags.push_back({ XOR("LETHAL"), { 252, 161, 3, low_alpha } });
				}
				else if (m_weapon && !m_weapon->IsKnife()) {
					auto data = m_weapon->GetWpnData();
					if (data->m_damage >= (int)std::round(player->m_iHealth()))
						flags.push_back({ XOR("LETHAL"), Color(252, 103, 3, low_alpha) });
				}
			}

			if (*it == 9) {
				if (data && data->m_records.size() && !dormant) {
					LagRecord* current = data->m_records.front().get();
					if (!game::IsFakePlayer(current->m_player->index())
						&& current->m_body != current->m_eye_angles.y) {
						flags.push_back({ XOR("FAKE"), { 255,255,255, low_alpha } });
					}
				}
			}

		}

		// iterate flags.
		for (size_t i{ }; i < flags.size(); ++i) {
			// get flag job (pair).
			const auto& f = flags[i];

			int offset = i * (render::esp_small.m_size.m_height - 1);

			// draw flag.
			render::esp_small.string(box.x + box.w + 2, box.y + offset, f.second, f.first);
		}
	}

	// draw bottom bars.
	{
		int  offset{ 3 };

		// draw lby update bar.
		if (enemy && g_menu.main.players.lby_timer.get()) {
			AimPlayer* data = &g_aimbot.m_players[player->index() - 1];

			// make sure everything is valid.
			if (data && data->m_moved && data->m_records.size()) {
				// grab lag record.
				LagRecord* current = data->m_records.front().get();

				if (current) {
					if (!(current->m_velocity.length_2d() > 0.1 && !current->m_fake_walk) && data->m_body_index <= 3) {
						// calculate box width
						float cycle = std::clamp<float>(data->m_body_update - current->m_anim_time, 0.f, 1.1f);
						float width = (box.w * cycle) / 1.1f;

						//	if (width > 0.f) {
							// draw.
						render::rect_filled(box.x, box.y + box.h + offset - 1, box.w, 4, { 10, 10, 10, low_alpha });

						Color clr = g_menu.main.players.lby_timer_color.get();
						render::rect(box.x + 1, box.y + box.h + 3, width, 2, determine_clr(player, clr, alpha).malpha(g_menu.main.players.lby_timer_color.get().a()));
						// move down the offset to make room for the next bar.
						offset += 4;
						//	}
					}
				}
			}
		}

		// draw weapon.
		if ((enemy) || (!enemy && g_menu.main.players.teammates.get())) {
			Weapon* weapon = player->GetActiveWeapon();
			if (weapon) {
				WeaponInfo* data = weapon->GetWpnData();
				if (data) {
					int bar;
					float scale;

					// the maxclip1 in the weaponinfo
					int max = data->m_max_clip1;
					int current = weapon->m_iClip1();

					C_AnimationLayer* layer1 = &player->m_AnimOverlay()[1];

					// set reload state.
					bool reload = (layer1->m_weight != 0.f) && (player->GetSequenceActivity(layer1->m_sequence) == 967);

					// ammo bar.
					if (max != -1 && g_menu.main.players.ammo_bar.get()) {
						// check for reload.
						if (reload)
							scale = layer1->m_cycle;

						// not reloading.
						// make the division of 2 ints produce a float instead of another int.
						else
							scale = (float)current / max;

						// relative to bar.
						bar = (int)std::round((box.w - 2) * scale);

						// draw.

						Color clr = g_menu.main.players.ammo_bar_color.get();
						render::rect_filled(box.x - 1, box.y + box.h + offset - 1, box.w + 2, 4, determine_clr(player, Color(0, 0, 0, g_menu.main.players.ammo_bar_color.get().a()), low_alpha).malpha(g_menu.main.players.ammo_bar_color.get().a()));
						render::rect(box.x, box.y + box.h + offset, bar + 2, 2, determine_clr(player, clr, alpha).malpha(g_menu.main.players.ammo_bar_color.get().a()));

						// less then a 5th of the bullets left.
						if (current > 0 && current <= int(std::floor(float(max) * 0.9f)) && !reload)

							render::esp_small.string(box.x + bar, (box.y + box.h + offset) - 3, determine_clr(player, Color(255, 255, 255, g_menu.main.players.ammo_bar_color.get().a()), low_alpha).malpha(g_menu.main.players.ammo_bar_color.get().a()), std::to_string(current), render::ALIGN_CENTER);

						offset += 3;
					}

					if (g_menu.main.players.distance.get()) {
						render::esp_small.string(box.x + (box.w / 2), box.y + box.h + offset, determine_clr(player, Color(g_menu.main.players.distance_color.get().r(), g_menu.main.players.distance_color.get().g(), g_menu.main.players.distance_color.get().b(), g_menu.main.players.distance_color.get().a()), low_alpha).malpha(g_menu.main.players.distance_color.get().a()), distance, render::ALIGN_CENTER);
						offset += render::esp_small.m_size.m_height;
					}

					// text.
					if (g_menu.main.players.text_weapon.get()) {
						// construct std::string instance of localized weapon name.
						std::string name{ weapon->GetLocalizedName() };

						// smallfonts needs upper case.
						std::transform(name.begin(), name.end(), name.begin(), ::toupper);

						render::esp_small.string(box.x + (box.w / 2), box.y + box.h + offset, determine_clr(player, Color(g_menu.main.players.text_weaponcol.get().r(), g_menu.main.players.text_weaponcol.get().g(), g_menu.main.players.text_weaponcol.get().b(), g_menu.main.players.text_weaponcol.get().a()), low_alpha).malpha(g_menu.main.players.text_weaponcol.get().a()), name, render::ALIGN_CENTER);
						offset += render::esp_small.m_size.m_height - 1;
					}

					// icons.
					if (g_menu.main.players.weapon_icon.get()) {
						render::cs.string(box.x + (box.w / 2), box.y + box.h + offset, determine_clr(player, Color(g_menu.main.players.weapon_iconcol.get().r(), g_menu.main.players.weapon_iconcol.get().g(), g_menu.main.players.weapon_iconcol.get().b(), g_menu.main.players.weapon_iconcol.get().a()), low_alpha).malpha(g_menu.main.players.weapon_iconcol.get().a()), GetWeaponIcon(weapon->m_iItemDefinitionIndex()), render::ALIGN_CENTER);
						offset += render::cs.m_size.m_height - 1;
					}
				}
			}
		}
	}
}

void Visuals::DrawPlantedC4() {
	bool        mode_2d, mode_3d, is_visible;
	float       explode_time_diff, dist, range_damage;
	vec3_t      dst, to_target;
	int         final_damage;
	std::string time_str, damage_str;
	Color       damage_color;
	vec2_t      screen_pos;

	static auto scale_damage = [](float damage, int armor_value) {
		float new_damage, armor;

		if (armor_value > 0) {
			new_damage = damage * 0.5f;
			armor = (damage - new_damage) * 0.5f;

			if (armor > (float)armor_value) {
				armor = (float)armor_value * 2.f;
				new_damage = damage - armor;
			}

			damage = new_damage;
		}

		return std::max(0, (int)std::floor(damage));
	};

	// store menu vars for later.
	mode_2d = g_menu.main.visuals.planted_c4.get(0);
	mode_3d = g_menu.main.visuals.planted_c4.get(1);
	if (!mode_2d && !mode_3d)
		return;

	// bomb not currently active, do nothing.
	if (!m_c4_planted)
		return;

	// calculate bomb damage.
	// references:
	//     https://github.com/VSES/SourceEngine2007/blob/43a5c90a5ada1e69ca044595383be67f40b33c61/se2007/game/shared/cstrike/weapon_c4.cpp#L271
	//     https://github.com/VSES/SourceEngine2007/blob/43a5c90a5ada1e69ca044595383be67f40b33c61/se2007/game/shared/cstrike/weapon_c4.cpp#L437
	//     https://github.com/ValveSoftware/source-sdk-2013/blob/master/sp/src/game/shared/sdk/sdk_gamerules.cpp#L173
	{
		// get our distance to the bomb.
		// todo - dex; is dst right? might need to reverse CBasePlayer::BodyTarget...
		dst = g_cl.m_local->WorldSpaceCenter();
		to_target = m_planted_c4_explosion_origin - dst;
		dist = to_target.length();

		// calculate the bomb damage based on our distance to the C4's explosion.
		range_damage = m_planted_c4_damage * std::exp((dist * dist) / ((m_planted_c4_radius_scaled * -2.f) * m_planted_c4_radius_scaled));

		// now finally, scale the damage based on our armor (if we have any).
		final_damage = scale_damage(range_damage, g_cl.m_local->m_ArmorValue());
	}

	// m_flC4Blow is set to gpGlobals->curtime + m_flTimerLength inside CPlantedC4.
	explode_time_diff = m_planted_c4_explode_time - g_csgo.m_globals->m_curtime;

	// get formatted strings for bomb.
	time_str = tfm::format(XOR("%.2f"), explode_time_diff);
	damage_str = tfm::format(XOR("%i"), final_damage);

	// get damage color.
	damage_color = (final_damage < g_cl.m_local->m_iHealth()) ? colors::white : colors::red;

	// finally do all of our rendering.
	is_visible = render::WorldToScreen(m_planted_c4_explosion_origin, screen_pos);

	std::string bomb = m_last_bombsite.c_str();

	// 'on screen (2D)'.
	if (mode_2d) {
		std::string timer1337 = tfm::format(XOR("%s - %.1fs"), bomb.substr(0, 1), explode_time_diff);
		std::string damage1337 = tfm::format(XOR("%i"), final_damage);

		Color colortimer = { 135, 172, 10, 255 };
		if (explode_time_diff < 10) colortimer = { 200, 200, 110, 255 };
		if (explode_time_diff < 5) colortimer = { 192, 32, 17, 255 };

		if (m_c4_planted && !bombexploded && !bombedefused) {
			if (explode_time_diff > 0.f) {
				render::indicator.string(6, 11, { 0,0, 0, 125 }, timer1337);
				render::indicator.string(5, 10, colortimer, timer1337);
			}
			//Render.StringCustom(5, 0, 0, getSite(c4) + timer + "s", colortimer, font);
			if (g_cl.m_local->m_iHealth() <= final_damage) {
				render::indicator.string(6, 31, { 0,0, 0, 125 }, tfm::format(XOR("FATAL")));
				render::indicator.string(5, 30, { 192, 32, 17, 255 }, tfm::format(XOR("FATAL")));
			}
			else if (final_damage > 1) {
				render::indicator.string(5, 31, { 0,0, 0, 125 }, tfm::format(XOR("- %iHP"), damage1337));
				render::indicator.string(6, 30, { 255, 255, 152, 255 }, tfm::format(XOR("- %iHP"), damage1337));
			}
		}
	}

	// 'on bomb (3D)'.
	if (mode_3d && is_visible) {
		if (explode_time_diff > 0.f)
			render::esp_small.string(screen_pos.x, screen_pos.y, colors::white, time_str, render::ALIGN_CENTER);

		// only render damage string if we're alive.
		if (g_cl.m_local->alive())
			render::esp_small.string(screen_pos.x, (int)screen_pos.y + render::esp_small.m_size.m_height, damage_color, damage_str, render::ALIGN_CENTER);
	}
}

bool Visuals::GetPlayerBoxRect(Player* player, Rect& box) {
	vec3_t min, max, out_vec;
	float left, bottom, right, top;
	matrix3x4_t& tran_frame = player->m_pCoordFrame();

	// get hitbox bounds.
	min = player->m_vecMins();
	max = player->m_vecMaxs();

	vec2_t screen_boxes[8];

	// transform mins and maxes to points. 
	vec3_t points[] =
	{
		{ min.x, min.y, min.z },
		{ min.x, max.y, min.z },
		{ max.x, max.y, min.z },
		{ max.x, min.y, min.z },
		{ max.x, max.y, max.z },
		{ min.x, max.y, max.z },
		{ min.x, min.y, max.z },
		{ max.x, min.y, max.z }
	};

	// transform points to 3-dimensional space.
	for (int i = 0; i <= 7; i++) {
		math::VectorTransform(points[i], tran_frame, out_vec);
		if (!render::WorldToScreen(out_vec, screen_boxes[i]))
			return false;
	}

	// generate an array to clamp later.
	vec2_t box_array[] = {
		screen_boxes[3],
		screen_boxes[5],
		screen_boxes[0],
		screen_boxes[4],
		screen_boxes[2],
		screen_boxes[1],
		screen_boxes[6],
		screen_boxes[7]
	};

	// state the position and size of the box.
	left = screen_boxes[3].x,
		bottom = screen_boxes[3].y,
		right = screen_boxes[3].x,
		top = screen_boxes[3].y;

	// clamp the box sizes.
	for (int i = 0; i <= 7; i++) {
		if (left > box_array[i].x)
			left = box_array[i].x;

		if (bottom < box_array[i].y)
			bottom = box_array[i].y;

		if (right < box_array[i].x)
			right = box_array[i].x;

		if (top > box_array[i].y)
			top = box_array[i].y;
	}

	// state the box bounds.
	box.x = left;
	box.y = top;
	box.w = right - left;
	box.h = (bottom - top);

	return true;
}

void Visuals::DrawHistorySkeleton(Player* player, int opacity) {
	const model_t* model;
	studiohdr_t* hdr;
	mstudiobone_t* bone;
	AimPlayer* data;
	LagRecord* record;
	int           parent;
	vec3_t        bone_pos, parent_pos;
	vec2_t        bone_pos_screen, parent_pos_screen;


	// get player's model.
	model = player->GetModel();
	if (!model)
		return;

	// get studio model.
	hdr = g_csgo.m_model_info->GetStudioModel(model);
	if (!hdr)
		return;

	data = &g_aimbot.m_players[player->index() - 1];
	if (!data)
		return;

	record = g_resolver.FindLastRecord(data);
	if (!record)
		return;

	for (int i{ }; i < hdr->m_num_bones; ++i) {
		// get bone.
		bone = hdr->GetBone(i);
		if (!bone || !(bone->m_flags & BONE_USED_BY_HITBOX))
			continue;

		// get parent bone.
		parent = bone->m_parent;
		if (parent == -1)
			continue;

		// resolve main bone and parent bone positions.
		record->m_bones->get_bone(bone_pos, i);
		record->m_bones->get_bone(parent_pos, parent);

		Color clr = player->enemy(g_cl.m_local) ? g_menu.main.players.skeleton_history_col.get() : g_menu.main.players.skeleton_history_col.get();

		// world to screen both the bone parent bone then draw.
		if (render::WorldToScreen(bone_pos, bone_pos_screen) && render::WorldToScreen(parent_pos, parent_pos_screen))
			render::line(bone_pos_screen.x, bone_pos_screen.y, parent_pos_screen.x, parent_pos_screen.y, determine_clr(player, clr, opacity).malpha(g_menu.main.players.skeleton_history_col.get().a()));
	}
}

void Visuals::DrawSkeleton(Player* player, int opacity) {
	const model_t* model;
	studiohdr_t* hdr;
	mstudiobone_t* bone;
	int           parent;
	BoneArray     matrix[128];
	vec3_t        bone_pos, parent_pos;
	vec2_t        bone_pos_screen, parent_pos_screen;

	// get player's model.
	model = player->GetModel();
	if (!model)
		return;

	// get studio model.
	hdr = g_csgo.m_model_info->GetStudioModel(model);
	if (!hdr)
		return;

	// get bone matrix.
	if (!player->SetupBones(matrix, 128, BONE_USED_BY_ANYTHING, g_csgo.m_globals->m_curtime))
		return;

	for (int i{ }; i < hdr->m_num_bones; ++i) {
		// get bone.
		bone = hdr->GetBone(i);
		if (!bone || !(bone->m_flags & BONE_USED_BY_HITBOX))
			continue;

		// get parent bone.
		parent = bone->m_parent;
		if (parent == -1)
			continue;

		// resolve main bone and parent bone positions.
		matrix->get_bone(bone_pos, i);
		matrix->get_bone(parent_pos, parent);

		Color clr = player->enemy(g_cl.m_local) ? g_menu.main.players.skeleton_color.get() : g_menu.main.players.skeleton_color.get();

		// world to screen both the bone parent bone then draw.
		if (render::WorldToScreen(bone_pos, bone_pos_screen) && render::WorldToScreen(parent_pos, parent_pos_screen))
			render::line(bone_pos_screen.x, bone_pos_screen.y, parent_pos_screen.x, parent_pos_screen.y, determine_clr(player, clr, opacity).malpha(g_menu.main.players.skeleton_color.get().a()));
	}
}

bool is_grenade(const int id)
{
	return id == 9 || id == 98 || id == 134;
}

void Visuals::RenderGlow() {
	Color   color;
	Player* player;

	if (!g_cl.m_local)
		return;

	if (!g_csgo.m_glow->m_object_definitions.Count())
		return;

	float blend = g_menu.main.players.glow_color.get().a() / 255.f;

	for (int i{ }; i < g_csgo.m_glow->m_object_definitions.Count(); ++i) {
		GlowObjectDefinition_t* obj = &g_csgo.m_glow->m_object_definitions[i];

		if (obj->IsUnused() || !obj->m_entity)
			continue;

		const auto classid = obj->m_entity->GetClientClass()->m_ClassID;

		if (is_grenade(classid) && g_menu.main.visuals.proj.get() && g_menu.main.visuals.proj_adds.get(1)) {
			color = g_menu.main.visuals.proj_glow_color.get();

			float blend = g_menu.main.visuals.proj_color.get().a() / 100.f;

			obj->m_render_occluded = true;
			obj->m_render_unoccluded = false;
			obj->m_render_full_bloom = false;
			obj->m_color = { (float)color.r() / 255.f, (float)color.g() / 255.f, (float)color.b() / 255.f };
			obj->m_alpha = blend;
			obj->m_bloom_amount = 1.f;
		}

		if (obj->m_entity->IsBaseCombatWeapon() && g_menu.main.visuals.items_adds.get(1) && g_menu.main.visuals.items.get()) {
			color = g_menu.main.visuals.item_color.get();
			float blend = g_menu.main.visuals.item_color.get().a() / 255.f;

			obj->m_render_occluded = true;
			obj->m_render_unoccluded = false;
			obj->m_render_full_bloom = false;
			obj->m_color = { (float)color.r() / 255.f, (float)color.g() / 255.f, (float)color.b() / 255.f };
			obj->m_alpha = blend;
			obj->m_bloom_amount = 1.f;
		}

		if (obj->m_entity->IsPlayer()) {

			// skip non-players.
			if (!obj->m_entity || !obj->m_entity->IsPlayer())
				continue;

			// get player ptr.
			player = obj->m_entity->as< Player* >();

			if (player->m_bIsLocalPlayer())
				continue;

			// get reference to array variable.
			float& opacity = m_opacities[player->index() - 1];

			bool enemy = player->enemy(g_cl.m_local);

			if (enemy && !g_menu.main.players.glow.get())
				continue;

			if (!enemy && !g_menu.main.players.teammates.get())
				continue;

			// enemy color
			if (enemy)
				color = g_menu.main.players.glow_color.get();

			// friendly color
			else
				color = g_menu.main.players.glow_color.get();

			obj->m_render_occluded = true;
			obj->m_render_unoccluded = false;
			obj->m_render_full_bloom = false;
			obj->m_color = { (float)color.r() / 255.f, (float)color.g() / 255.f, (float)color.b() / 255.f };
			obj->m_alpha = opacity * blend;
		}
	}
}

void Visuals::DrawHitboxMatrix(LagRecord* record, float time) {
	if (g_menu.main.players.matrix_shot.get() == 0)
		return;

	const model_t* model;
	studiohdr_t* hdr;
	mstudiohitboxset_t* set;
	mstudiobbox_t* bbox;
	vec3_t             mins, maxs, origin;
	ang_t			   angle;

	model = record->m_player->GetModel();
	if (!model)
		return;

	hdr = g_csgo.m_model_info->GetStudioModel(model);
	if (!hdr)
		return;

	set = hdr->GetHitboxSet(record->m_player->m_nHitboxSet());
	if (!set)
		return;

	bool behind_walls = g_menu.main.players.matrix_behind_wall.get();

	if (g_menu.main.players.matrix_shot.get() == 2) {
		bbox = set->GetHitbox(g_aimbot.m_hitbox);

		if (!bbox)
			return;


		Color col = g_menu.main.players.matrix_shot_clr_target.get();

		// bbox.
		if (bbox->m_radius <= 0.f) {
			// https://developer.valvesoftware.com/wiki/Rotation_Tutorial

			// convert rotation angle to a matrix.
			matrix3x4_t rot_matrix;
			g_csgo.AngleMatrix(bbox->m_angle, rot_matrix);

			// apply the rotation to the entity input space (local).
			matrix3x4_t matrix;
			math::ConcatTransforms(record->m_bones[bbox->m_bone], rot_matrix, matrix);

			// extract the compound rotation as an angle.
			ang_t bbox_angle;
			math::MatrixAngles(matrix, bbox_angle);

			// extract hitbox origin.
			vec3_t origin = matrix.GetOrigin();

			// draw box.
			g_csgo.m_debug_overlay->AddBoxOverlay(origin, bbox->m_mins, bbox->m_maxs, bbox_angle, col.r(), col.g(), col.b(), 0, time);
		}

		// capsule.
		else {
			// NOTE; the angle for capsules is always 0.f, 0.f, 0.f.

			// create a rotation matrix.
			matrix3x4_t matrix;
			g_csgo.AngleMatrix(bbox->m_angle, matrix);

			// apply the rotation matrix to the entity output space (world).
			math::ConcatTransforms(record->m_bones[bbox->m_bone], matrix, matrix);

			// get world positions from new matrix.
			math::VectorTransform(bbox->m_mins, matrix, mins);
			math::VectorTransform(bbox->m_maxs, matrix, maxs);

			g_csgo.m_debug_overlay->AddCapsuleOverlay(mins, maxs, bbox->m_radius, col.r(), col.g(), col.b(), col.a(), time, 0, behind_walls);
		}

		return;
	}

	for (int i{ }; i < set->m_hitboxes; ++i) {

		bbox = set->GetHitbox(i);
		if (!bbox)
			continue;

		Color col = i == g_aimbot.m_hitbox
			&& g_menu.main.players.matrix_shot.get() == 3 ?
			g_menu.main.players.matrix_shot_clr_target.get() : g_menu.main.players.matrix_shot_clr.get();

		// bbox.
		if (bbox->m_radius <= 0.f) {
			// https://developer.valvesoftware.com/wiki/Rotation_Tutorial

			// convert rotation angle to a matrix.
			matrix3x4_t rot_matrix;
			g_csgo.AngleMatrix(bbox->m_angle, rot_matrix);

			// apply the rotation to the entity input space (local).
			matrix3x4_t matrix;
			math::ConcatTransforms(record->m_bones[bbox->m_bone], rot_matrix, matrix);

			// extract the compound rotation as an angle.
			ang_t bbox_angle;
			math::MatrixAngles(matrix, bbox_angle);

			// extract hitbox origin.
			vec3_t origin = matrix.GetOrigin();

			// draw box.
			g_csgo.m_debug_overlay->AddBoxOverlay(origin, bbox->m_mins, bbox->m_maxs, bbox_angle, col.r(), col.g(), col.b(), 0, time);
		}

		// capsule.
		else {
			// NOTE; the angle for capsules is always 0.f, 0.f, 0.f.

			// create a rotation matrix.
			matrix3x4_t matrix;
			g_csgo.AngleMatrix(bbox->m_angle, matrix);

			// apply the rotation matrix to the entity output space (world).
			math::ConcatTransforms(record->m_bones[bbox->m_bone], matrix, matrix);

			// get world positions from new matrix.
			math::VectorTransform(bbox->m_mins, matrix, mins);
			math::VectorTransform(bbox->m_maxs, matrix, maxs);

			g_csgo.m_debug_overlay->AddCapsuleOverlay(mins, maxs, bbox->m_radius, col.r(), col.g(), col.b(), col.a(), time, 0, behind_walls);
		}
	}
}

//void Visuals::DrawBeams() {
//	;
//	size_t     impact_count;
//	float      curtime, dist;
//	bool       is_final_impact;
//	vec3_t     va_fwd, start, dir, end;
//	BeamInfo_t beam_info;
//	Beam_t* beam;
//	bool enemyshot = false;
//	static float shotdata[65];
//	vec3_t shotpos;
//
//
//	if (!g_cl.m_local)
//		return;
//
//	if (!g_menu.main.visuals.impact_beams.get(0))
//		return;
//
//	auto vis_impacts = &g_shots.m_vis_impacts;
//
//	// the local player is dead, clear impacts.
//	if (!g_cl.m_processing) {
//		if (!vis_impacts->empty())
//			vis_impacts->clear();
//	}
//
//	else {
//		impact_count = vis_impacts->size();
//		if (!impact_count)
//			return;
//
//		curtime = game::TICKS_TO_TIME(g_cl.m_local->m_nTickBase());
//
//		for (size_t i{ impact_count }; i-- > 0; ) {
//			auto impact = &vis_impacts->operator[ ](i);
//			if (!impact)
//				continue;
//
//			// impact is too old, erase it.
//			if (std::abs(curtime - game::TICKS_TO_TIME(impact->m_tickbase)) > g_menu.main.visuals.impact_beams_time.get()) {
//				vis_impacts->erase(vis_impacts->begin() + i);
//
//				continue;
//			}
//
//			// already rendering this impact, skip over it.
//			if (impact->m_ignore)
//				continue;
//
//			// is this the final impact?
//			// last impact in the vector, it's the final impact.
//			if (i == (impact_count - 1))
//				is_final_impact = true;
//
//			// the current impact's tickbase is different than the next, it's the final impact.
//			else if ((i + 1) < impact_count && impact->m_tickbase != vis_impacts->operator[ ](i + 1).m_tickbase)
//				is_final_impact = true;
//
//			else
//				is_final_impact = false;
//
//			// is this the final impact?
//			//is_final_impact = ( ( i == ( impact_count - 1 ) ) || ( impact->m_tickbase != vis_impacts->at( i + 1 ).m_tickbase ) );
//			bool monkey = true;
//			if (is_final_impact) {
//				// calculate start and end position for beam.
//				start = impact->m_shoot_pos;
//
//				dir = (impact->m_impact_pos - start).normalized();
//				dist = (impact->m_impact_pos - start).length();
//
//				end = start + (dir * dist);
//
//				// this is used to invert the start and end placement of the bullet tracors so the animation looks like it goes from the gun and to the player instead of the other way around
//				vec3_t startend = end;
//				vec3_t endstart = start;
//
//				// setup beam info.
//				// note - dex; possible beam models: sprites/physbeam.vmt | sprites/white.vmt
//				beam_info.m_vecStart = startend;
//				beam_info.m_vecEnd = endstart; // if i want to invert the animations then i just have to change this to start and the start to end (and fix the jitter ofc)
//				if (g_menu.main.visuals.impact_beams_type.get() == 0) {
//					beam_info.m_nModelIndex = g_csgo.m_model_info->GetModelIndex(XOR("sprites/white.vmt"));
//					beam_info.m_pszModelName = XOR("sprites/purplelaser1.vmt");
//					beam_info.m_flHaloScale = 0.f;
//					beam_info.m_flLife = g_menu.main.visuals.impact_beams_time.get();
//					beam_info.m_flWidth = 2.f;
//					beam_info.m_flEndWidth = 2.f;
//					beam_info.m_flFadeLength = 0.f;
//					beam_info.m_flAmplitude = 0.f;   // beam 'jitter'.
//					beam_info.m_flBrightness = 255.f;
//					beam_info.m_flSpeed = 0.5f;  // seems to control how fast the 'scrolling' of beam is... once fully spawned.
//					beam_info.m_nStartFrame = 0;
//					beam_info.m_flFrameRate = 0.f;
//					beam_info.m_nSegments = 2;     // controls how much of the beam is 'split up', usually makes m_flAmplitude and m_flSpeed much more noticeable.
//					beam_info.m_bRenderable = true;  // must be true or you won't see the beam.
//					beam_info.m_nFlags = 0;
//				}
//				else if (g_menu.main.visuals.impact_beams_type.get() == 1) {
//					//beam_info.m_nType = 0;
//					beam_info.m_pszModelName = "sprites/physbeam.vmt";
//					beam_info.m_nModelIndex = -1;
//					beam_info.m_flHaloScale = 0.0f;
//					beam_info.m_flLife = g_menu.main.visuals.impact_beams_time.get();
//
//					beam_info.m_flFadeLength = 0.0f;
//					beam_info.m_flAmplitude = 0.0f;
//					beam_info.m_nSegments = 2;
//					beam_info.m_bRenderable = true;
//					beam_info.m_flBrightness = 255.f;
//					beam_info.m_flSpeed = 0.2f;
//					beam_info.m_nStartFrame = 0;
//					beam_info.m_flFrameRate = 0.f;
//					beam_info.m_flWidth = 2.0f;
//					beam_info.m_flEndWidth = 2.0f;
//
//					beam_info.m_nFlags = 0;
//				}
//				else {
//					//beam_info.m_nType = 0;
//					beam_info.m_pszModelName = "sprites/purplelaser1.vmt";
//					beam_info.m_nModelIndex = g_csgo.m_model_info->GetModelIndex(XOR("sprites/purplelaser1.vmt"));
//					beam_info.m_flHaloScale = 0.0f;
//					beam_info.m_flLife = g_menu.main.visuals.impact_beams_time.get();
//
//					beam_info.m_flFadeLength = 0.0f;
//					beam_info.m_flAmplitude = 0.0f;
//					beam_info.m_nSegments = 2;
//					beam_info.m_bRenderable = true;
//					beam_info.m_flBrightness = 180.f;
//					beam_info.m_flSpeed = 0.2f;
//					beam_info.m_nStartFrame = 0;
//					beam_info.m_flFrameRate = 1.f;
//					beam_info.m_flWidth = 6.0f;
//					beam_info.m_flEndWidth = 6.0f;
//
//					beam_info.m_nFlags = 0x8300;
//				}
//
//				if (!impact->m_hit_player) {
//					beam_info.m_flRed = g_menu.main.visuals.impact_beams_color.get().r();
//					beam_info.m_flGreen = g_menu.main.visuals.impact_beams_color.get().g();
//					beam_info.m_flBlue = g_menu.main.visuals.impact_beams_color.get().b();
//				}
//
//				// attempt to render the beam.
//				if (g_menu.main.visuals.impact_beams_type.get() != 3) {
//					beam = g_csgo.m_beams->CreateBeamPoints(beam_info);
//					if (beam) {
//						g_csgo.m_beams->DrawBeam(beam);
//					}
//				}
//				// attempt to render the line.
//				else if (g_menu.main.visuals.impact_beams_type.get() == 3) {
//					g_csgo.m_debug_overlay->AddLineOverlay(start, end, beam_info.m_flRed, beam_info.m_flGreen, beam_info.m_flBlue, false, g_menu.main.visuals.impact_beams_time.get());
//				}
//
//				// we only want to render a beam for this impact once.
//				impact->m_ignore = true;
//			}
//		}
//	}
//}

//void Visuals::DebugAimbotPoints(Player* player) {
//	std::vector< vec3_t > p2{ };
//
//	AimPlayer* data = &g_aimbot.m_players.at(player->index() - 1);
//	if (!data || data->m_records.empty())
//		return;
//
//	LagRecord* front = data->m_records.front().get();
//	if (!front || front->dormant())
//		return;
//
//	// get bone matrix.
//	BoneArray matrix[128];
//	if (!g_bones.setup(player, matrix, front))
//		return;
//
//	data->SetupHitboxes(front, false);
//	if (data->m_hitboxes.empty())
//		return;
//
//	for (const auto& it : data->m_hitboxes) {
//		std::vector< vec3_t > p1{ };
//
//		if (!data->SetupHitboxPoints(front, matrix, it.m_index, p1))
//			continue;
//
//		for (auto& p : p1)
//			p2.push_back(p);
//	}
//
//	if (p2.empty())
//		return;
//
//	for (auto& p : p2) {
//		vec2_t screen;
//
//		if (render::WorldToScreen(p, screen))
//			render::rect_filled(screen.x, screen.y, 2, 2, { 0, 255, 255, 255 });
//	}
//}

//void Visuals::Hitmarker3D()
//{
//
//	if (!g_menu.main.misc.hitmarker3dd.get())
//		return;
//
//	if (hitmarkers.size() == 0)
//		return;
//
//	// draw
//	for (int i = 0; i < hitmarkers.size(); i++) {
//		vec3_t pos3D = vec3_t(hitmarkers[i].impact.x, hitmarkers[i].impact.y, hitmarkers[i].impact.z);
//		vec2_t pos2D;
//
//		if (!render::WorldToScreen(pos3D, pos2D))
//			continue;
//
//		int r = 255;
//		int g = 255;
//		int b = 255;
//
//		//render::line(pos2D.x + 6, pos2D.y + 6, pos2D.x + 10, pos2D.y + 10, { r, g, b, hitmarkers[i].alpha });
//		//render::line(pos2D.x - 6, pos2D.y - 6, pos2D.x - 10, pos2D.y - 10, { r, g, b, hitmarkers[i].alpha });
//		//render::line(pos2D.x + 6, pos2D.y - 6, pos2D.x + 10, pos2D.y - 10, { r, g, b, hitmarkers[i].alpha });
//		//render::line(pos2D.x - 6, pos2D.y + 6, pos2D.x - 10, pos2D.y + 10, { r, g, b, hitmarkers[i].alpha });
//
//		render::rect_filled(pos2D.x / 2 + 6, pos2D.y / 2 + 6, 1, 1, { r, g, b, hitmarkers[i].alpha });
//		render::rect_filled(pos2D.x / 2 + 7, pos2D.y / 2 + 7, 1, 1, { r, g, b, hitmarkers[i].alpha });
//		render::rect_filled(pos2D.x / 2 + 8, pos2D.y / 2 + 8, 1, 1, { r, g, b, hitmarkers[i].alpha });
//		render::rect_filled(pos2D.x / 2 + 9, pos2D.y / 2 + 9, 1, 1, { r, g, b, hitmarkers[i].alpha });
//		render::rect_filled(pos2D.x / 2 + 10, pos2D.y / 2 + 10, 1, 1, { r, g, b, hitmarkers[i].alpha });
//
//		render::rect_filled(pos2D.x / 2 - 6, pos2D.y / 2 - 6, 1, 1, { r, g, b, hitmarkers[i].alpha });
//		render::rect_filled(pos2D.x / 2 - 7, pos2D.y / 2 - 7, 1, 1, { r, g, b, hitmarkers[i].alpha });
//		render::rect_filled(pos2D.x / 2 - 8, pos2D.y / 2 - 8, 1, 1, { r, g, b, hitmarkers[i].alpha });
//		render::rect_filled(pos2D.x / 2 - 9, pos2D.y / 2 - 9, 1, 1, { r, g, b, hitmarkers[i].alpha });
//		render::rect_filled(pos2D.x / 2 - 10, pos2D.y / 2 - 10, 1, 1, { r, g, b, hitmarkers[i].alpha });
//
//		render::rect_filled(pos2D.x / 2 - 6, pos2D.y / 2 + 6, 1, 1, { r, g, b, hitmarkers[i].alpha });
//		render::rect_filled(pos2D.x / 2 - 7, pos2D.y / 2 + 7, 1, 1, { r, g, b, hitmarkers[i].alpha });
//		render::rect_filled(pos2D.x / 2 - 8, pos2D.y / 2 + 8, 1, 1, { r, g, b, hitmarkers[i].alpha });
//		render::rect_filled(pos2D.x / 2 - 9, pos2D.y / 2 + 9, 1, 1, { r, g, b, hitmarkers[i].alpha });
//		render::rect_filled(pos2D.x / 2 - 10, pos2D.y / 2 + 10, 1, 1, { r, g, b, hitmarkers[i].alpha });
//
//		render::rect_filled(pos2D.x / 2 + 6, pos2D.y / 2 - 6, 1, 1, { r, g, b, hitmarkers[i].alpha });
//		render::rect_filled(pos2D.x / 2 + 7, pos2D.y / 2 - 7, 1, 1, { r, g, b, hitmarkers[i].alpha });
//		render::rect_filled(pos2D.x / 2 + 8, pos2D.y / 2 - 8, 1, 1, { r, g, b, hitmarkers[i].alpha });
//		render::rect_filled(pos2D.x / 2 + 9, pos2D.y / 2 - 9, 1, 1, { r, g, b, hitmarkers[i].alpha });
//		render::rect_filled(pos2D.x / 2 + 10, pos2D.y / 2 - 10, 1, 1, { r, g, b, hitmarkers[i].alpha });
//
//		// proceed
//		for (int i = 0; i < hitmarkers.size(); i++) {
//			if (hitmarkers[i].time + 1.25f <= g_csgo.m_globals->m_curtime) {
//				hitmarkers[i].alpha -= 1;
//			}
//
//			if (hitmarkers[i].alpha <= 0)
//				hitmarkers.erase(hitmarkers.begin() + i);
//		}
//	}
//
//
//}
