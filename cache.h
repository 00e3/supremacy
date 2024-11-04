#pragma once

using UpdateClientSideAnimation_t = bool(__thiscall*)(void* player);

class cached_data {
public:
	struct {
		UpdateClientSideAnimation_t UpdateClientSideAnimation;
	} m_functions;
};

extern cached_data g_cache;

