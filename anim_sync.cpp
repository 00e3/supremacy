#include "includes.h"

void AimPlayer::CorrectVelocity(Player* player, LagRecord* record, LagRecord* previous_record) {
	if (previous_record) {
		auto was_in_air = !((player->m_fFlags() & FL_ONGROUND) && (previous_record->m_flags & FL_ONGROUND));
		auto animation_speed = 0.0f;
		auto origin_delta = record->m_origin - previous_record->m_origin;

		vec3_t vecPreviousOrigin = previous_record->m_origin;
		int fPreviousFlags = previous_record->m_flags;
		auto time_difference = std::max(g_csgo.m_globals->m_interval, game::TICKS_TO_TIME(record->m_lag));

		// fix velocity
		// https://github.com/VSES/SourceEngine2007/blob/master/se2007/game/client/c_baseplayer.cpp#L659
		if (!origin_delta.is_zero() && game::TIME_TO_TICKS(time_difference) > 0 && game::TIME_TO_TICKS(time_difference) < 16) {
			//record->m_vecVelocity = (record->m_vecOrigin - previous_record->m_vecOrigin) * (1.f / record->m_flChokeTime);
			record->m_anim_velocity = origin_delta * (1.f / time_difference);

			if (player->m_fFlags() & FL_ONGROUND
				&& record->m_layers[11].m_weight > 0.0f
				&& record->m_layers[11].m_weight < 1.0f
				&& record->m_layers[11].m_cycle > previous_record->m_layers[11].m_cycle)
			{
				auto weapon = player->m_hActiveWeapon().Get();

				if (weapon)
				{
					auto max_speed = 260.0f;
					auto weapon = player->GetActiveWeapon();
					auto weapon_info = weapon->GetWpnData();

					if (weapon_info)
						max_speed = player->m_bIsScoped() ? weapon_info->m_max_player_speed_alt : weapon_info->m_max_player_speed;

					auto modifier = 0.35f * (1.0f - record->m_layers[11].m_weight);

					if (modifier > 0.0f && modifier < 1.0f)
						animation_speed = max_speed * (modifier + 0.55f);
				}
			}

			if (animation_speed > 0.0f)
			{
				// maybe use normal vel (?)
				animation_speed /= record->m_anim_velocity.length_2d();
				record->m_anim_velocity.x *= animation_speed;
				record->m_anim_velocity.y *= animation_speed;
			}

			if (this->m_records.size() >= 3 && time_difference > g_csgo.m_globals->m_interval)
			{
				auto previous_velocity = (previous_record->m_origin - this->m_records.at(2).get()->m_origin) * (1.0f / time_difference);

				// was !was_in_air, should be was_in_air
				// https://cdn.discordapp.com/attachments/1041049699848814672/1070045115512541274/image.png
				// ^ corrected using eexomi.idb
				// p100 antipaste right here ft. Geico HvH $
				if (!previous_velocity.is_zero() && was_in_air)
				{

					// PS: since im too lazy to reverse i use animvel
					// but could be real vel, who cares i dont
					auto current_direction = math::normalize_float(math::rad_to_deg(atan2(record->m_anim_velocity.y, record->m_anim_velocity.x)));
					auto previous_direction = math::normalize_float(math::rad_to_deg(atan2(previous_velocity.y, previous_velocity.x)));

					auto average_direction = current_direction - previous_direction;
					average_direction = math::deg_to_rad(math::normalize_float(current_direction + average_direction * 0.5f));

					auto direction_cos = cos(average_direction);
					auto dirrection_sin = sin(average_direction);

					// maybe use raw velocity ? idk
					// auto velocity_speed = record->m_vecVelocity.Length2D();
					auto velocity_speed = record->m_anim_velocity.length_2d();

					record->m_anim_velocity.x = direction_cos * velocity_speed;
					record->m_anim_velocity.y = dirrection_sin * velocity_speed;
				}
			}

			// fix CGameMovement::FinishGravity
			if (!(player->m_fFlags() & FL_ONGROUND)) {
				auto fixed_timing = std::clamp(time_difference, g_csgo.m_globals->m_interval, 1.0f);
				record->m_anim_velocity.z -= g_csgo.sv_gravity->GetFloat() * fixed_timing * 0.5f;
			}
			else
				record->m_anim_velocity.z = 0.0f;
		}
	}
}