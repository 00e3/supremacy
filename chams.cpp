#include "includes.h"

Chams g_chams{ };;

void Chams::SetColor(Color col, IMaterial* mat) {
	if (mat)
		mat->ColorModulate(col);

	else
		g_csgo.m_render_view->SetColorModulation(col);
}

void Chams::SetAlpha(float alpha, IMaterial* mat) {
	if (mat)
		mat->AlphaModulate(alpha);

	else
		g_csgo.m_render_view->SetBlend(alpha);
}

void Chams::SetupMaterial(IMaterial* mat, Color col, bool z_flag) {
	SetColor(col);

	// mat->SetFlag(  MATERIAL_VAR_HALFLAMBERT, flags  );
	mat->SetFlag(MATERIAL_VAR_ZNEARER, z_flag);
	mat->SetFlag(MATERIAL_VAR_NOFOG, z_flag);
	mat->SetFlag(MATERIAL_VAR_IGNOREZ, z_flag);

	g_csgo.m_studio_render->ForcedMaterialOverride(mat);
}

void Chams::init() {

	std::ofstream("csgo\\materials\\tap_reflect.vmt") << R"#("VertexLitGeneric" {
					"$basetexture"				"vgui/white_additive"
					"$ignorez"					"0"
					"$phong"					"1"
					"$BasemapAlphaPhongMask"    "1"
					"$phongexponent"			"15"
					"$normalmapalphaenvmask"	"1"
					"$envmap"					"env_cubemap"
					"$envmaptint"				"[0.0 0.0 0.0]"
					"$phongboost"				"[0.6 0.6 0.6]"
					"phongfresnelranges"		"[0.5 0.5 1.0]"
					"$nofog"					"1"
					"$model"					"1"
					"$nocull"					"0"
					"$selfillum"				"1"
					"$halflambert"				"1"
					"$znearer"					"0"
					"$flat"						"0"	
					"$rimlight"					"1"
					"$rimlightexponent"			"2"
					"$rimlightboost"			"0"
        })#";

	// find stupid materials.
	debugambientcube = g_csgo.m_material_system->FindMaterial(XOR("debug/debugambientcube"), XOR("Model textures"));
	debugambientcube->IncrementReferenceCount();

	debugdrawflat = g_csgo.m_material_system->FindMaterial(XOR("debug/debugdrawflat"), XOR("Model textures"));
	debugdrawflat->IncrementReferenceCount();

	metallic = g_csgo.m_material_system->FindMaterial(XOR("tap_reflect"), XOR("Model textures"));
	metallic->IncrementReferenceCount();
}

bool Chams::GenerateLerpedMatrix(int index, BoneArray* out) {
	LagRecord* current_record;
	AimPlayer* data;

	Player* ent = g_csgo.m_entlist->GetClientEntity< Player* >(index);
	if (!ent)
		return false;

	if (!g_aimbot.IsValidTarget(ent))
		return false;

	data = &g_aimbot.m_players[index - 1];
	if (!data || data->m_records.empty())
		return false;

	if (data->m_records.size() < 2)
		if (data->m_records.front().get()->m_broke_lc)
			return false;

	if (data->m_records.size() < 3)
		return false;

	auto* channel_info = g_csgo.m_engine->GetNetChannelInfo();
	if (!channel_info)
		return false;

	float max_unlag = g_csgo.m_cvar->FindVar(HASH("sv_maxunlag"))->GetFloat();

	// start from begin
	for (auto it = data->m_records.begin(); it != data->m_records.end(); ++it) {

		LagRecord* last_first{ nullptr };
		LagRecord* last_second{ nullptr };

		if (it->get()->valid() && it + 1 != data->m_records.end() && !(it + 1)->get()->valid() && !(it + 1)->get()->dormant()) {
			last_first = (it + 1)->get();
			last_second = (it)->get();
		}

		if (!last_first || !last_second)
			continue;

		const auto& FirstInvalid = last_first;
		const auto& LastInvalid = last_second;

		if (!LastInvalid || !FirstInvalid)
			continue;

		const auto NextOrigin = LastInvalid->m_origin;
		const auto curtime = g_csgo.m_globals->m_curtime;

		auto flDelta = 1.f - (curtime - LastInvalid->m_interp_time) / (LastInvalid->m_sim_time - FirstInvalid->m_sim_time);
		if (flDelta < 0.f || flDelta > 1.f)
			LastInvalid->m_interp_time = curtime;

		flDelta = 1.f - (curtime - LastInvalid->m_interp_time) / (LastInvalid->m_sim_time - FirstInvalid->m_sim_time);

		//const auto lerp = math::Interpolate(NextOrigin, FirstInvalid->m_origin, std::clamp(flDelta, 0.f, 1.f));
		const auto lerp = NextOrigin;


		matrix3x4_t ret[128];
		std::memcpy(ret, FirstInvalid->m_bones, sizeof(ret));

		for (size_t i{ }; i < 128; ++i) {
			const auto matrix_delta = FirstInvalid->m_bones[i].GetOrigin() - FirstInvalid->m_origin;
			ret[i].SetOrigin(matrix_delta + lerp);
		}

		std::memcpy(out, ret, sizeof(ret));
		return true;
	}

	return false;
}

void Chams::DrawChams(void* ecx, uintptr_t ctx, const DrawModelState_t& state, const ModelRenderInfo_t& info, matrix3x4_t* bone) {
	if (strstr(info.m_model->m_name, XOR("models/player")) != nullptr) {
		Player* m_entity = g_csgo.m_entlist->GetClientEntity<Player*>(info.m_index);
		if (!m_entity || !m_entity->alive() || m_entity->m_iHealth() < 0)
			return;

		g_csgo.m_studio_render->ForcedMaterialOverride(nullptr);
		g_csgo.m_render_view->SetColorModulation(colors::white);
		g_csgo.m_render_view->SetBlend(1.f);

		if ((g_menu.main.players.chams_style.get() == 2 && m_entity)) {
			if (metallic) {
				Color color_mettal = g_menu.main.players.chams_enemy_reflectivity.get();

				//auto phong = metallic->find_var(XOR("$phongboost"));
				//if (phong)
				//	phong->set_float(g_menu.main.players.chams_enemy_reflectivity_shine.get() / 100.f);

				//auto rim = metallic->find_var(XOR("$rimlightboost"));
				//if (rim)
				//	rim->set_float(g_menu.main.players.chams_enemy_reflectivity_rim.get() / 100.f);

				auto reflectivity = metallic->find_var(XOR("$envmaptint"));
				if (reflectivity)
					reflectivity->set_vector(vec3_t(color_mettal.r() / 255.f, color_mettal.g() / 255.f, color_mettal.b() / 255.f));

			}

			int dadadaa = 0;
		}

		if (m_entity == g_cl.m_local && g_cl.m_processing && g_cl.m_local) {
			if (g_menu.main.players.chams_local.get()) {
				if (metallic) {
					Color color_mettal = g_menu.main.players.chams_enemy_reflectivity.get();
					//auto phong = metallic->find_var(XOR("$phongboost"));
					//if (phong)
					//	phong->set_float(g_menu.main.players.chams_enemy_reflectivity_shine.get() / 100.f);

					//auto rim = metallic->find_var(XOR("$rimlightboost"));
					//if (rim)
					//	rim->set_float(g_menu.main.players.chams_enemy_reflectivity_rim.get() / 100.f);

					auto reflectivity = metallic->find_var(XOR("$envmaptint"));
					if (reflectivity)
						reflectivity->set_vector(vec3_t(color_mettal.r() / 255.f, color_mettal.g() / 255.f, color_mettal.b() / 255.f));
				}

				// override blend.
				Color color = g_menu.main.players.chams_local_col.get();

				SetAlpha(g_menu.main.players.chams_local_col.get().a() / 255.f);
				// set material and color.
				switch (g_menu.main.players.chams_local_style.get()) {
				case 0:
					SetupMaterial(debugambientcube, color, false);
					break;
				case 1:
					SetupMaterial(debugdrawflat, color, false);
					break;
				case 2:
					SetupMaterial(metallic, color, false);
					break;
				default:
					break;
				}

				if (g_menu.main.players.chams_local_scope.get() && m_entity->m_bIsScoped())
					SetAlpha(g_menu.main.players.chams_local_scope_amt.get() / 100.f);

				g_hooks.m_model_render.GetOldMethod< Hooks::DrawModelExecute_t >(IVModelRender::DRAWMODELEXECUTE)(ecx, ctx, state, info, bone);
			}

			if (g_menu.main.players.chams_local_scope.get() && m_entity->m_bIsScoped())
				SetAlpha(g_menu.main.players.chams_local_scope_amt.get() / 100.f);
		}

		bool enemy = g_cl.m_local && m_entity->enemy(g_cl.m_local);

		if (enemy && g_menu.main.players.chams_enemy_history.get()) {
			if (g_aimbot.IsValidTarget(m_entity)) {
				AimPlayer* data = &g_aimbot.m_players[m_entity->index() - 1];
				if (data && data->m_records.size() > 2) {
					LagRecord* record = g_resolver.FindLastRecord(data);
					if (record) {
						// was the matrix properly setup?
						BoneArray arr[128];
						if (Chams::GenerateLerpedMatrix(m_entity->index(), arr)) {
							// override blend.
							SetAlpha(g_menu.main.players.chams_enemy_history_col.get().a() / 255.f);

							// set material and color.
							//SetupMaterial(debugdrawflat, g_menu.main.players.chams_enemy_history_col.get(), true);
							switch (g_menu.main.players.chams_history_style.get()) {
							case 0:
								SetupMaterial(debugambientcube, g_menu.main.players.chams_enemy_history_col.get(), true);
								break;
							case 1:
								SetupMaterial(debugdrawflat, g_menu.main.players.chams_enemy_history_col.get(), true);
								break;
							case 2:
								SetupMaterial(metallic, g_menu.main.players.chams_enemy_history_col.get(), true);
								break;
							default:
								break;
							}

							g_hooks.m_model_render.GetOldMethod< Hooks::DrawModelExecute_t >(IVModelRender::DRAWMODELEXECUTE)(ecx, ctx, state, info, arr);
							g_csgo.m_render_view->SetBlend(1);
							g_csgo.m_studio_render->ForcedMaterialOverride(nullptr);
							debugdrawflat->SetFlag(MATERIAL_VAR_ZNEARER, false);
							debugdrawflat->SetFlag(MATERIAL_VAR_NOFOG, false);
							debugdrawflat->SetFlag(MATERIAL_VAR_IGNOREZ, false);
						}
					}
				}
			}
		}

		if ((enemy && g_menu.main.players.chams_enemy_vis_enable.get() || (!enemy && g_menu.main.players.chams_friendly_vis_enable.get()))) {
			if (g_menu.main.players.chams_enemy_invis_enable.get()) {

				SetAlpha(g_menu.main.players.chams_enemy_invis.get().a() / 255.f);

				switch (g_menu.main.players.chams_style.get()) {
				case 0:
					SetupMaterial(debugambientcube, g_menu.main.players.chams_enemy_invis.get(), true);
					break;
				case 1:
					SetupMaterial(debugdrawflat, g_menu.main.players.chams_enemy_invis.get(), true);
					break;
				case 2:
					SetupMaterial(metallic, g_menu.main.players.chams_enemy_invis.get(), true);
					break;
				default:
					break;
				}

				g_hooks.m_model_render.GetOldMethod< Hooks::DrawModelExecute_t >(IVModelRender::DRAWMODELEXECUTE)(ecx, ctx, state, info, bone);
			}

			SetAlpha(g_menu.main.players.chams_enemy_vis.get().a() / 255.f);

			switch (g_menu.main.players.chams_style.get()) {
			case 0:
				SetupMaterial(debugambientcube, g_menu.main.players.chams_enemy_vis.get(), false);
				break;
			case 1:
				SetupMaterial(debugdrawflat, g_menu.main.players.chams_enemy_vis.get(), false);
				break;
			case 2:
				SetupMaterial(metallic, g_menu.main.players.chams_enemy_vis.get(), false);
				break;
			default:
				break;
			}

			g_hooks.m_model_render.GetOldMethod< Hooks::DrawModelExecute_t >(IVModelRender::DRAWMODELEXECUTE)(ecx, ctx, state, info, bone);
		}
	}
}

void Chams::SceneEnd() {
	// store and sort ents by distance.
	if (SortPlayers()) {
	}

	// restore.
	g_csgo.m_studio_render->ForcedMaterialOverride(nullptr);
	g_csgo.m_render_view->SetColorModulation(colors::white);
	g_csgo.m_render_view->SetBlend(1.f);
}

bool Chams::IsInViewPlane(const vec3_t& world) {
	float w;

	const VMatrix& matrix = g_csgo.m_engine->WorldToScreenMatrix();

	w = matrix[3][0] * world.x + matrix[3][1] * world.y + matrix[3][2] * world.z + matrix[3][3];

	return w > 0.001f;
}

bool Chams::OverridePlayer(int index) {
	Player* player = g_csgo.m_entlist->GetClientEntity< Player* >(index);
	if (!player)
		return false;

	// always skip the local player in DrawModelExecute.
	// this is because if we want to make the local player have less alpha
	// the static props are drawn after the players and it looks like aids.
	// therefore always process the local player in scene end.
	if (player->m_bIsLocalPlayer())
		return true;

	// see if this player is an enemy to us.
	bool enemy = g_cl.m_local && player->enemy(g_cl.m_local);

	// we have chams on enemies.
	if (enemy && (g_menu.main.players.chams_enemy_vis_enable.get() || g_menu.main.players.chams_enemy_invis_enable.get()))
		return true;

	// we have chams on friendly.
	else if (!enemy && g_menu.main.players.chams_friendly_vis_enable.get())
		return true;

	return false;
}

bool Chams::SortPlayers() {
	// lambda-callback for std::sort.
	// to sort the players based on distance to the local-player.
	static auto distance_predicate = [](Entity* a, Entity* b) {
		vec3_t local = g_cl.m_local->GetAbsOrigin();

		// note - dex; using squared length to save out on sqrt calls, we don't care about it anyway.
		float len1 = (a->GetAbsOrigin() - local).length_sqr();
		float len2 = (b->GetAbsOrigin() - local).length_sqr();

		return len1 < len2;
	};

	// reset player container.
	m_players.clear();

	// find all players that should be rendered.
	for (int i{ 1 }; i <= g_csgo.m_globals->m_max_clients; ++i) {
		// get player ptr by idx.
		Player* player = g_csgo.m_entlist->GetClientEntity< Player* >(i);

		// validate.
		if (!player || !player->IsPlayer() || !player->alive() || player->m_iHealth() < 0)
			continue;

		// do not draw players occluded by view plane.
		if (!IsInViewPlane(player->WorldSpaceCenter()))
			continue;

		// this player was not skipped to draw later.
		// so do not add it to our render list.
		if (!OverridePlayer(i))
			continue;

		m_players.push_back(player);
	}

	// any players?
	if (m_players.empty())
		return false;

	// sorting fixes the weird weapon on back flickers.
	// and all the other problems regarding Z-layering in this shit game.
	std::sort(m_players.begin(), m_players.end(), distance_predicate);

	return true;
}