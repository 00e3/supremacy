#pragma once

class AimPlayer;

enum return_type_t : int {
	RETURN_NOTHING = 0,
	RETURN_DELAY = 1,
	RETURN_NO_LC = 2,
	RETURN_LC = 3
};

class LagCompensation {
public:
	int StartPrediction(AimPlayer* player);
};

extern LagCompensation g_lagcomp;