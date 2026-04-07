#include "bg_tarascii_move.h"
#include "qcommon/q_shared.h"
#include "bg_public.h"

#if defined(_GAME)
#include "g_tarascii_main.h"
#endif

// Clientside prediction for the barrel movement. Eliminates any perceived latency to your barrel when moving around.
void Tarascii_BarrelMove(playerState_t *predictedPS)
{
	bgEntity_t *barrel;

   	if (predictedPS->duelIndex != ENTITYNUM_NONE && predictedPS->duelIndex != 0)
	{
		barrel = PM_BGEntForNum(predictedPS->duelIndex);
		VectorCopy( predictedPS->origin, barrel->s.origin );
		barrel->s.origin[2] -= 24;
		VectorCopy( barrel->s.origin, barrel->s.pos.trBase );

		barrel->s.angles[1] = barrel->s.apos.trBase[1] = predictedPS->viewangles[1];

#if defined(_GAME)
		Tarascii_BarrelNonSolid(predictedPS->duelIndex);
		trap->LinkEntity((sharedEntity_t*)barrel);
#endif
	}
}
