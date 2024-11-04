#include "includes.h"

LagCompensation g_lagcomp{};;

int LagCompensation::StartPrediction(AimPlayer* entry) {
    // we have no data to work with.
    // this should never happen if we call this
    if (entry->m_records.empty())
        return RETURN_DELAY;

    // get first record.
    LagRecord* record = entry->m_records.front().get();

    record->predict();

    // why would we be able to hit?
    if (entry->m_player->dormant()
        || record->m_dormant
        || !record->m_setup
        || record->m_lag < 0)
        return RETURN_DELAY;

    LagRecord* previous = entry->m_records.size() >= 2 ? entry->m_records[1].get() : nullptr;

    if (record->shift_type() == shift_type_t::SHIFT_BREAK_LC)
        return RETURN_NO_LC;

    if (entry->m_records.size() <= 2) {
        record->m_method = 1;
        return RETURN_LC;
    }

    // should be a possible fix for people using fluc or dynamic idfk XD?
    if (previous)
    {
        const vec3_t previous_net_delta = previous->m_origin - previous->m_old_origin;

        if (previous_net_delta.length_2d_sqr() > 4096.f)
            record->m_broke_lc = true;
    }

    const vec3_t net_delta = record->m_origin - record->m_old_origin;

    // check if lc broken.
    if (net_delta.length_2d_sqr() > 4096.f)
        record->m_broke_lc = true;

    // just use server-sided lagcomp if this is the case
    if (!record->m_broke_lc)
        return RETURN_NO_LC;

    int simulation = game::TIME_TO_TICKS(record->m_sim_time);

    if (!record->valid()) // could probably work to reduce lagfix issues for people with high ping, dting or ping spiking above 200
        return RETURN_LC;

    if (record->m_lag > 16) {
        record->m_method = 2;
        return RETURN_LC;
    }

    if (previous) {
        // compute the amount of lag that we will predict for, if we have one set of data, use that.
        // if we have more data available, use the previous lag delta to counter weird fakelags that switch between 16 and 2.
        int lag = game::TIME_TO_TICKS(record->m_sim_time - previous->m_sim_time);

        // clamp this just to be sure.
        math::clamp(lag, 1, 17);

        // get the delta in ticks between the last server net update
        // and the net update on which we created this record.
        int updatedelta = g_cl.m_server_tick - record->m_tick;

        if (!updatedelta) {
            record->m_method = 3;
            return RETURN_LC;
        }

        const int receive_tick = std::abs(g_cl.m_arrival_tick - game::TIME_TO_TICKS(record->m_sim_time));

        // too much lag to predict, exit and delay shot
        if (receive_tick / record->m_lag > 19)
            return RETURN_DELAY;

        const float adjusted_arrive_tick = game::TIME_TO_TICKS(g_cl.m_latency + g_csgo.m_globals->m_realtime - record->m_time);

        // too much lag to predict, exit and delay shot
        if (adjusted_arrive_tick - record->m_lag >= 0)
            return RETURN_DELAY;

        record->m_method = 4;

        return RETURN_LC;
    }
    else {

        return RETURN_DELAY;
    }
}