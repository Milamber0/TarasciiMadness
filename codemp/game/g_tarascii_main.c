#include "g_tarascii_main.h"
#include "qcommon/q_tarascii_shared.h"

#define DEBUGBARRELS 0

tarasciiState_t g_tState;

// File-scope statics for incremental barrel spawning
static vec3_t s_barrelPositions[MAX_GENTITIES];
static int s_pendingSpawnCount = 0;
static int s_pendingSpawnNext = 0;
void Tarascii_ContinueBarrelPlacement(void); // forward declare

qboolean Tarascii_CanPlayerJoin()
{
	return (qboolean)(g_tState.AllowJoin || tm_debugAllowJoin.integer != 0);
}

void Tarascii_Init(int levelTime)
{
	// Preserve startingBarrel via cvar — globals reset when DLL reloads on map_restart
	int prevBarrel = trap->Cvar_VariableIntegerValue("tm_prevBarrel");
	memset( &g_tState, 0, sizeof( g_tState ) );
	g_tState.startingBarrel = (prevBarrel >= 0) ? prevBarrel : -1;
	s_pendingSpawnCount = 0;
	s_pendingSpawnNext = 0;

	g_weaponDisable.integer = 524279;							// Has to be set this way or else gun pickups are still turned on during the first round.
	trap->Cvar_Set("timelimit", va("%i", timelimit.integer));	// Fix to make sure timelimit works when starting the map for the first time, seems to be a bug where it assumes 0 unless it gets set again.
	trap->Cvar_Set("disable_item_force_boon", "1");
	trap->Cvar_Set("disable_item_force_enlighten_light", "1");
	trap->Cvar_Set("disable_item_force_enlighten_dark", "1");
	trap->Cvar_Set("disable_item_medpak_instant", "1");
	trap->Cvar_Set("disable_item_medpac", "1");
	trap->Cvar_Set("disable_item_shield_sm_instant", "1");
	trap->Cvar_Set("disable_item_shield_lrg_instant", "1");
	trap->Cvar_Set("fraglimit", "0");
	//trap->Cvar_Set("g_forcePowerDisable", "245759");			// Disable every forcepower except seeing.
}

void Tarascii_ReadSessionData()
{
	int i;
	char sessionData[MAX_CVAR_VALUE_STRING] = {0};

	trap->Cvar_VariableStringBuffer( "sessionTarasciiNumRounds", sessionData, sizeof(sessionData) );
	g_tState.sessionNumRounds = atoi(sessionData);

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		trap->Cvar_VariableStringBuffer( va("sessionTarasciiHasClients%i", i), sessionData, sizeof(sessionData) );
		g_tState.clientVersions[i] = atoi(sessionData);
	}

	for (int i = 0; i < level.maxclients; i++) {
		if (level.clients[i].pers.connected == CON_CONNECTED) {
			char			s[MAX_CVAR_VALUE_STRING] = { 0 };
			const char* var;
			gclient_t* client = &level.clients[i];
			Tarascii_ReadClientSessionData(client);
		}
	}
}

void Tarascii_ReadClientSessionData(gclient_t* client)
{
	char			s[MAX_CVAR_VALUE_STRING] = { 0 };
	const char* var;
	var = va("sessionTarasciiClient%i", client - level.clients);
	trap->Cvar_VariableStringBuffer(var, s, sizeof(s));
	sscanf(s, "%i", &client->sess.isAdmin);
}

void Tarascii_WriteSessionData()
{
	int i;
	trap->Cvar_Set("sessionTarasciiNumRounds", va("%i", g_tState.sessionNumRounds));

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		trap->Cvar_Set(va("sessionTarasciiHasClients%i", i), va("%i", g_tState.clientVersions[i]));
	}

	for (int i = 0; i < level.maxclients; i++) {
		if (level.clients[i].pers.connected == CON_CONNECTED) {
			char		s[MAX_CVAR_VALUE_STRING] = { 0 };
			const char* var;
			gclient_t* client = &level.clients[i];
			Tarascii_WriteClientSessionData(client);
		}
	}

}

void Tarascii_WriteClientSessionData(gclient_t* client)
{
	char		s[MAX_CVAR_VALUE_STRING] = { 0 };
	const char* var;
	var = va("sessionTarasciiClient%i", client - level.clients);
	Q_strcat(s, sizeof(s), va("%i ", client->sess.isAdmin));
	trap->Cvar_Set(var, s);
}

void Tarascii_pickFirstBarrel()
{
	// Per-client debugForceTeam: if someone forced themselves to barrel, pick them
	for (int i = 0; i < level.maxclients; i++) {
		if (g_tState.debugForceTeam[i] == 1 &&
			g_entities[i].client && g_entities[i].client->pers.connected == CON_CONNECTED) {
			g_tState.startingBarrel = i;
				trap->Cvar_Set("tm_prevBarrel", va("%d", i));
			return;
		}
	}

	// Build list of connected players who aren't the previous barrel
	int candidates[MAX_CLIENTS];
	int numCandidates = 0;

	for (int i = 0; i < level.maxclients; i++) {
		if (g_entities[i].client && g_entities[i].client->pers.connected == CON_CONNECTED
			&& i != g_tState.startingBarrel) {
			candidates[numCandidates++] = i;
		}
	}


	if (numCandidates > 0) {
		g_tState.startingBarrel = candidates[(int)(random() * numCandidates) % numCandidates];
	} else {
		// Fallback: only one player, or no one else available
		for (int i = 0; i < level.maxclients; i++) {
			if (g_entities[i].client && g_entities[i].client->pers.connected == CON_CONNECTED) {
				g_tState.startingBarrel = i;
				break;
			}
		}
	}
	trap->Cvar_Set("tm_prevBarrel", va("%d", g_tState.startingBarrel));
}

team_t Tarascii_GetTeam(gentity_t* ent)
{
	team_t result;

	// Late joiners (not present at round start) always join barrel team
	if (g_tState.startingBarrel == ent->s.clientNum || !g_tState.presentAtStart[ent->s.clientNum]) {
		result = TEAM_RED;
	} else {
		result = TEAM_BLUE;
	}

	// Per-client debugForceTeam override
	if (g_tState.debugForceTeam[ent->s.clientNum] == 1) {
		result = TEAM_RED;
	}
	else if (g_tState.debugForceTeam[ent->s.clientNum] == 2) {
		result = TEAM_BLUE;
	}

	return result;
}

void Tarascii_RunFrame()
{
	int i;
	int connectedClients = 0;

	// Incrementally spawn pending barrels
	Tarascii_ContinueBarrelPlacement();

	// Survivors win celebration — invulnerability + double fire rate for humans
	if (g_tState.survivorsWon) {
		for (int ci = 0; ci < MAX_CLIENTS; ci++) {
			if (g_entities[ci].client && g_entities[ci].client->pers.connected == CON_CONNECTED
				&& g_entities[ci].client->sess.sessionTeam == TEAM_BLUE) {
				g_entities[ci].client->ps.eFlags |= EF_INVULNERABLE;
				g_entities[ci].client->invulnerableTimer = level.time + 1000;
				g_entities[ci].client->ps.userInt3 = 1; // signal for double fire rate in bg_pmove
			}
		}
	}

	g_tState.barrelTeamCounter = g_tState.humanTeamCounter = 0;

	// Counts players on each team and checks how many are still connecting.
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (Q_stricmp(g_entities[i].classname,"player") == 0)
		{
			float pluginVersionTimer = g_tState.clientVersionTimers[i];

			if (g_entities[i].client->sess.sessionTeam == TEAM_RED)
			{
				g_tState.barrelTeamCounter++;
			}
			else if (g_entities[i].client->sess.sessionTeam == TEAM_BLUE)
			{
				g_tState.humanTeamCounter++;
			}

			if (g_entities[i].client->pers.connected == CON_CONNECTED)
			{
				connectedClients++;
			}

			//TarasciiMadness client plugin missing message.
			//// Sends message to clients who dont have a client plugin installed or it's outdated.
			//if(pluginVersionTimer > 0)
			//{
			//	if(level.time >= pluginVersionTimer)
			//	{
			//		if(g_tState.clientVersions[i] <= 0)
			//		{
			//			// Center print
			//			trap->SendServerCommand( i, "cp \"No plugin detected.\n");

			//			// Private message
			//			trap->SendServerCommand( i, va("chat \"Server: ^6No plugin detected. You're advised to install it for a smoother experience.\" %i", i));
			//		}

			//		if(g_tState.clientVersions[i] > 0 && g_tState.clientVersions[i] < CLIENT_VERSION)
			//		{
			//			// Center print
			//			trap->SendServerCommand( i, "cp \"Outdated plugin detected.\n");

			//			// Private message
			//			trap->SendServerCommand( i, va("chat \"Server: ^6Outdated plugin detected. Server is running version ^1%i^6. Updating the plugin is recommended.\" %i", CLIENT_VERSION, i));
			//		}

			//		// Stop the timer
			//		g_tState.clientVersionTimers[i] = 0;
			//	}
			//}
		}
	}

	// Starts the timers for starting the game, only called once when there's enough players.
	// level.numConnectedClients doesn't work because it includes CON_CONNECTING too.
	if (!g_tState.timerSetOnce && connectedClients >= MIN_PLAYER_TO_START)
	{
		g_tState.timerSetOnce = qtrue;
		trap->SendServerCommand( -1, "cp \"We have enough players.\nThe game is about to start\"");
		g_tState.startTime = level.time + 5000;
		g_tState.winlossDelay = level.time + 7000;
	}


	// Checks if there are still at least 2 players connected and that the timer has been reached, then picks the starting barrel and allows players to join.
	if ( g_tState.AllowJoin == qfalse && g_tState.startTime != 0 && g_tState.startTime <= level.time)
	{
		if (connectedClients >= MIN_PLAYER_TO_START)
		{
			// Pick barrel now — all players are connected at this point
			Tarascii_pickFirstBarrel();
			g_tState.AllowJoin = qtrue;
			g_tState.winlossDelay = level.time + 5000;

			// Mark everyone currently connected — only they can be survivors
			for (int ci = 0; ci < MAX_CLIENTS; ci++) {
				if (g_entities[ci].client && g_entities[ci].client->pers.connected == CON_CONNECTED) {
					g_tState.presentAtStart[ci] = qtrue;
				}
			}
			// Reset the round timer so the full timelimit starts NOW, not from map load
			level.startTime = level.time;
			trap->SetConfigstring(CS_LEVEL_START_TIME, va("%i", level.startTime));
		}
		// If the timer has started but there is no longer enough players, let the timers reset.
		else
		{
			g_tState.timerSetOnce = qfalse;
		}
	}

	// Handling win/loss conditions that depend on either team having 0 members, winlossDelay added to make sure everyone has spawned first.
	if (level.numPlayingClients > 0 && g_tState.AllowJoin && level.time >= g_tState.winlossDelay)
	{
		if (g_tState.humanTeamCounter == 0)
		{
			static int delayWin = 0;
			static int lastMsgTime = 0;
			if(delayWin == 0)
			{
				const char *name = (tm_barrelModelMode.integer == 2) ? "Eggs" : "Barrels";
				trap->SendServerCommand( -1, va("cp \"^2%s have won!\"", name));
				delayWin = level.time + 10000;
				lastMsgTime = level.time;
			}
			else if (level.time >= delayWin)
			{
				BeginIntermission();
				delayWin = 0;
				lastMsgTime = 0;
			}
			else if (level.time - lastMsgTime >= 3000)
			{
				// Re-send win message so it stays visible
				const char *name = (tm_barrelModelMode.integer == 2) ? "Eggs" : "Barrels";
				trap->SendServerCommand( -1, va("cp \"^2%s have won!\"", name));
				lastMsgTime = level.time;
			}

		}

		if (g_tState.barrelTeamCounter == 0)
		{
			static int delayReset = 0;
			if(delayReset == 0)
			{
				trap->SendServerCommand( -1, "cp \"No barrel player, returning to intermission.\"");
				delayReset = level.time + 5000;
			}
			else if (level.time >= delayReset)
			{
				BeginIntermission();
				delayReset = 0;
			}
		}
	}

}

void Tarascii_ChangeModel(int mode)
{
	for (int i = 0; i < MAX_GENTITIES; i++)
	{
		if (Q_stricmp(g_entities[i].classname, "tarasciiBarrel") == 0)
		{
			if (mode == 1)
			{
				g_entities[i].s.userInt1 = qtrue;
				g_entities[i].s.userInt2 = qfalse;
			}
			if (mode == 2)
			{
				g_entities[i].s.userInt1 = qfalse;
				g_entities[i].s.userInt2 = qtrue;
			}
		}
	}
}

void Tarascii_SpawnBarrel(vec3_t incPos)
{
	char mainModel[MAX_TOKEN_CHARS];
	char DisplayModel1[MAX_TOKEN_CHARS];
	char DisplayModel2[MAX_TOKEN_CHARS];
	char DisplayModel3[MAX_TOKEN_CHARS];
	char DisplayModel4[MAX_TOKEN_CHARS];
	char DisplayModel5[MAX_TOKEN_CHARS];
	gentity_t* obj;
	int size, rnd;
	vec3_t angles;

	// Check entity budget before spawning — leave a buffer for gameplay entities
	if (level.num_entities >= MAX_GENTITIES - 100) {
		return;
	}

	obj = G_Spawn();

	if ( !obj )
	{
		return;
	}

	obj->r.contents = CONTENTS_SOLID;
	obj->clipmask = 0;
	obj->s.eType = ET_MOVER;
	obj->classname = "tarasciiBarrel";

	Q_strncpyz(mainModel, "models/items/forcegem", sizeof(mainModel) );
	Q_strncpyz(DisplayModel1, "models/map_objects/factory/emod_big", sizeof(DisplayModel1));
	Q_strncpyz(DisplayModel2, "models/map_objects/tarascii_madness/egg", sizeof(DisplayModel2));
	Q_strncpyz(DisplayModel3, "models/map_objects/tarascii_madness/egg2", sizeof(DisplayModel3));
	Q_strncpyz(DisplayModel4, "models/map_objects/tarascii_madness/egg3", sizeof(DisplayModel4));
	Q_strncpyz(DisplayModel5, "models/map_objects/tarascii_madness/egg4", sizeof(DisplayModel5));
	char* model1 = G_NewString(va("%s.md3", DisplayModel1));
	char* model2 = G_NewString(va("%s.md3", DisplayModel2));
	char* model3 = G_NewString(va("%s.md3", DisplayModel3));
	char* model4 = G_NewString(va("%s.md3", DisplayModel4));
	char* model5 = G_NewString(va("%s.md3", DisplayModel5));

	obj->model = G_NewString(va ("%s.md3", mainModel));
	obj->s.modelindex = G_ModelIndex( obj->model );
	obj->model2 = model1;

	int scale = 10;
	if (tm_barrelModelMode.integer == 1)
	{
		obj->s.userInt1 = qtrue;
		obj->s.userInt2 = qfalse;
	}
	if (tm_barrelModelMode.integer == 2)
	{
		obj->s.userInt1 = qfalse;
		obj->s.userInt2 = qtrue;
	}
	obj->s.modelindex2 = G_ModelIndex(obj->model2);
	obj->s.boneIndex1 = G_ModelIndex(model1);
	obj->s.boneIndex2 = G_ModelIndex(model2);
	obj->s.boneIndex3 = G_ModelIndex(model3);
	obj->s.boneIndex4 = G_ModelIndex(model4);
	obj->s.boneOrient = G_ModelIndex(model5);
	obj->s.clientNum = 0;

	VectorCopy( incPos, obj->s.origin );
	VectorCopy( obj->s.origin, obj->s.pos.trBase );
	VectorCopy( obj->s.origin, obj->r.currentOrigin );

	VectorClear( angles );
	VectorCopy( angles, obj->s.angles );
	VectorCopy( angles, obj->r.currentAngles );
	VectorCopy( obj->s.angles, obj->s.apos.trBase );

	rnd = crandom() * 1000;
	obj->r.currentAngles[1] = rnd;
	obj->s.angles[1] = rnd;
	obj->s.apos.trBase[1] = rnd;

	size = 10;
	VectorSet (obj->r.mins, -1*size, -1*size, 0);
	VectorSet (obj->r.maxs, size, size, size*5);

	VectorSet(obj->modelScale, 1, 1, 1);
	obj->s.iModelScale = scale;

	trap->LinkEntity((sharedEntity_t*)obj);
}

void addToList(Vec3List_t* list, vec3_t coordinate)
{
	Vec3List_t *pointer = NULL;
	pointer = (Vec3List_t*)malloc(sizeof(Vec3List_t));

	// Make sure we're on the last element in the list.
	while (list->next != NULL)
	{
		list = list->next;
	}

	if (pointer != NULL)
	{
		list->next = pointer;
		memset(pointer,0,sizeof(*pointer));
		VectorCopy(coordinate, pointer->coordinate);
	}
}

void Tarascii_RandomJKALocation(vec3_t outLocation)
{
	// Randomize X and Y within map bounds (bounds already include padding)
	for (int i = 0; i < 2; i++)
	{
		float range = g_tState.mapBoundPointsMax[i] - g_tState.mapBoundPointsMin[i];
		outLocation[i] = range * (random()) + g_tState.mapBoundPointsMin[i];
	}

	// Z: start trace from above the highest map point
	outLocation[2] = g_tState.mapBoundPointsMax[2] + 500;
}

void Tarascii_FindMapBounds()
{
	int i;
	int entOffset = MAX_CLIENTS + BODY_QUEUE_SIZE;
	vec3_t zeroVec = {0,0,0};

	// First pass: find bounds from spawn points only (guaranteed in playable area)
	qboolean foundSpawn = qfalse;
	for (i = entOffset; i < level.num_entities; i++)
	{
		if (!Q_strncmp(g_entities[i].classname, "info_player", 11))
		{
			if (!foundSpawn) {
				VectorCopy(g_entities[i].s.origin, g_tState.mapBoundPointsMin);
				VectorCopy(g_entities[i].s.origin, g_tState.mapBoundPointsMax);
				foundSpawn = qtrue;
			} else {
				for (int j = 0; j < 3; j++) {
					if (g_entities[i].s.origin[j] > g_tState.mapBoundPointsMax[j])
						g_tState.mapBoundPointsMax[j] = g_entities[i].s.origin[j];
					if (g_entities[i].s.origin[j] < g_tState.mapBoundPointsMin[j])
						g_tState.mapBoundPointsMin[j] = g_entities[i].s.origin[j];
				}
			}
		}
	}

	// Fallback: if no spawn points, use all entities
	if (!foundSpawn)
	{
		VectorCopy(g_entities[entOffset].s.origin, g_tState.mapBoundPointsMin);
		VectorCopy(g_tState.mapBoundPointsMin, g_tState.mapBoundPointsMax);

		for (i = entOffset; i < level.num_entities; i++)
		{
			int j;
			if (!VectorCompare(g_entities[i].s.origin, zeroVec))
			{
				for (j = 0; j < 3; j++)
				{
					if (g_entities[i].s.origin[j] > g_tState.mapBoundPointsMax[j])
						g_tState.mapBoundPointsMax[j] = g_entities[i].s.origin[j];
					if (g_entities[i].s.origin[j] < g_tState.mapBoundPointsMin[j])
						g_tState.mapBoundPointsMin[j] = g_entities[i].s.origin[j];
				}
			}
		}
	}

	// Add padding around spawn-based bounds so barrels can spawn in adjacent rooms/areas.
	for (int j = 0; j < 2; j++) { // X and Y only
		float size = g_tState.mapBoundPointsMax[j] - g_tState.mapBoundPointsMin[j];
		float padding = (size < 1000) ? 2000 : size * 0.5f;
		g_tState.mapBoundPointsMin[j] -= padding;
		g_tState.mapBoundPointsMax[j] += padding;
	}
}

void Tarascii_BarrelPlacement()
{
	trace_t groundTrace, traceWalls;
	gentity_t *avoidEnt;
	qboolean avoidZone, validSurface = qfalse;
	int i, numEnts, entOffset, avoidTouch[MAX_GENTITIES];
	int barrelBuffer = 100;	// Buffer at the end of placing barrels to leave some ents left for rockets shots, sounds etc.
	int maxEntities = (MAX_GENTITIES - level.num_entities - barrelBuffer);
	int noRoomRetryCounter = 0;
	int noRoomMaxRetry = 2000; // How many consecutive failed tries before giving up.
	vec3_t downVec, avoidBoxMin, avoidBoxMax;
	vec3_t barrelBoundMin = {-10, -10, 0};
	vec3_t barrelBoundMax = {10, 10, 50};
	vec3_t barrelBoundLargerMin = {-15,-15,0};
	vec3_t barrelBoundLargerMax = {15,15,50};

	int numPositions = 0;
	int lastPrintCount = 0;
	float minBarrelDist = 60.0f;
	float minBarrelDistSq = minBarrelDist * minBarrelDist;

	Tarascii_FindMapBounds();	//Roughly finds the size of the level.
#if DEBUGBARRELS
	int next = 0;
	while (g_tState.spawnedBarrels <= 500 && noRoomRetryCounter < noRoomMaxRetry)
	{
		if (g_tState.spawnedBarrels % 10 == 0 && g_tState.spawnedBarrels != 0)
		{
			next++;
		}

		vec3_t spawnPoint;
		spawnPoint[0] = 55 * next;
		spawnPoint[1] = 35 * ((g_tState.spawnedBarrels % 10));
		spawnPoint[2] = 0;



		//
		//// Initial trace using the random location chosen.
		//trap->Trace(&groundTrace, spawnPoint, barrelBoundMin, barrelBoundMax, downVec, ENTITYNUM_NONE, MASK_SOLID, qfalse, 0, 0);

		g_tState.spawnedBarrels++;
		addToList(barrelPositionList, spawnPoint);
	}
#else
	Com_Printf("Barrel placement: populating up to %d barrels...\n", maxEntities);
	int placementStartTime = trap->Milliseconds();
	int placementTimeLimit = 30000; // max 30 seconds for barrel placement
	int totalAttempts = 0;
	int failNoGround = 0, failTrigger = 0, failSpawnPoint = 0, failWalls = 0, failDistance = 0;
	while (g_tState.spawnedBarrels < maxEntities && noRoomRetryCounter < noRoomMaxRetry)
	{
		totalAttempts++;
		// Time limit to prevent freezing the server
		if (trap->Milliseconds() - placementStartTime > placementTimeLimit) {
			break;
		}
		vec3_t spawnPoint;
		Tarascii_RandomJKALocation(spawnPoint);
		VectorCopy(spawnPoint, downVec);
		downVec[2] = -65536;

		// Trace down through all floors — collect walkable positions, then pick one randomly.
		vec3_t traceStart;
		VectorCopy(spawnPoint, traceStart);
		vec3_t walkablePositions[8]; // max floors to consider
		int numWalkable = 0;

		for (int bounce = 0; bounce < 10 && numWalkable < 8; bounce++)
		{
			trace_t tr;
			// Use a tiny hull instead of point trace — point traces can miss patch (curve) surfaces
			static vec3_t tinyMins = {-1, -1, 0};
			static vec3_t tinyMaxs = {1, 1, 1};
			trap->Trace(&tr, traceStart, tinyMins, tinyMaxs, downVec, ENTITYNUM_NONE, MASK_SOLID, qfalse, 0, 0);

			if (tr.fraction == 1.0f) break; // void

			if (tr.plane.normal[2] >= MIN_WALK_NORMAL &&
				!(tr.surfaceFlags & (SURF_SKY | SURF_NODRAW)))
			{
				// Verify barrel fits — check from above, then drop to ground
				trace_t fitTrace;
				vec3_t fitStart, fitEnd;
				VectorCopy(tr.endpos, fitStart);
				fitStart[2] += 50; // start well above to clear any slope
				VectorCopy(fitStart, fitEnd);
				fitEnd[2] = tr.endpos[2] - 1; // trace down to just below the surface

				trap->Trace(&fitTrace, fitStart, barrelBoundMin, barrelBoundMax, fitEnd, ENTITYNUM_NONE, MASK_SOLID, qfalse, 0, 0);

				if (fitTrace.fraction < 1.0f && !fitTrace.startsolid && !fitTrace.allsolid)
				{
					// Use the trace endpos — barrel sits right on the surface
					VectorCopy(fitTrace.endpos, walkablePositions[numWalkable]);
					numWalkable++;
				}
			}

			// Get past this surface to find more floors below
			vec3_t belowSurface;
			trace_t solidTrace;
			VectorCopy(tr.endpos, belowSurface);
			belowSurface[2] -= 256;

			trap->Trace(&solidTrace, tr.endpos, NULL, NULL, belowSurface, ENTITYNUM_NONE, MASK_SOLID, qfalse, 0, 0);

			if (solidTrace.fraction > 0.0f && !solidTrace.allsolid) {
				VectorCopy(solidTrace.endpos, traceStart);
				traceStart[2] -= 1;
			} else {
				VectorCopy(tr.endpos, traceStart);
				traceStart[2] -= 256;
			}
		}

		if (numWalkable == 0) { failNoGround++; continue; }

		// Pick a random floor from the ones found
		VectorCopy(walkablePositions[(int)(random() * numWalkable) % numWalkable], groundTrace.endpos);

		VectorClear(avoidBoxMin);
		VectorClear(avoidBoxMax);
		VectorAdd(groundTrace.endpos, barrelBoundLargerMin, avoidBoxMin);
		VectorAdd(groundTrace.endpos, barrelBoundLargerMax, avoidBoxMax);

		// Avoid Zone Check
		// Find all entities in the area, and make sure we're safe to spawn a barrel here.
		// In case of death triggers etc.
		numEnts = trap->EntitiesInBox( avoidBoxMin, avoidBoxMax, avoidTouch, MAX_GENTITIES );
		avoidZone = qfalse;
		for ( i = 0 ; i < numEnts ; i++ )
		{
			avoidEnt = &g_entities[avoidTouch[i]];
			if ( ( avoidEnt->r.contents & CONTENTS_TRIGGER ) )
			{
				// Only reject if the trigger is actually at barrel height.
				// Large kill planes under the map have their absmax[2] near the floor
				// but we don't want to reject valid walkable positions above them.
				// Use a point contents check at the actual barrel position instead.
				vec3_t testPoint;
				VectorCopy(groundTrace.endpos, testPoint);
				testPoint[2] += 25; // test at barrel center height
				int contents = trap->PointContents(testPoint, avoidTouch[i]);
				if (contents & CONTENTS_TRIGGER) {
					avoidZone = qtrue;
					break;
				}
			}
		}

		if(avoidZone)
		{
			failTrigger++;
			continue;
		}

		// Makes sure no barrel spawns too close to a spawn point so that spawning blue players can move.
		for (entOffset = MAX_CLIENTS + BODY_QUEUE_SIZE; entOffset < level.num_entities; entOffset++ )
		{
			avoidEnt = &g_entities[entOffset];
			if (!Q_strncmp(avoidEnt->classname, "info_player", 11))
			{
				if (Distance(groundTrace.endpos, avoidEnt->s.origin) < 100)
				{
					avoidZone = qtrue;
					break;
				}
			}
		}

		if(avoidZone)
		{
			failSpawnPoint++;
			continue;
		}


		// Walkability and fit already verified in bounce loop above.
		// Check wall clearance with slightly larger box, offset up to avoid clipping slopes
		{
			trace_t traceWalls;
			vec3_t wallCheckPos;
			VectorCopy(groundTrace.endpos, wallCheckPos);
			wallCheckPos[2] += 10; // lift above slope surface
			trap->Trace(&traceWalls, wallCheckPos, barrelBoundLargerMin, barrelBoundLargerMax, wallCheckPos, -1, MASK_SHOT, qfalse, 0, 0);
			if (traceWalls.startsolid || traceWalls.fraction != 1.0f) {
				failWalls++;
				noRoomRetryCounter++;
				continue;
			}
		}
		{
			qboolean isAvailable = qtrue;

			// Check distance to all previously placed barrels using squared distance
			for (int pi = 0; pi < numPositions; pi++)
			{
				vec3_t diff;
				VectorSubtract(groundTrace.endpos, s_barrelPositions[pi], diff);
				float distSq = diff[0]*diff[0] + diff[1]*diff[1] + diff[2]*diff[2];
				if (distSq < minBarrelDistSq)
				{
					isAvailable = qfalse;
					break;
				}
			}

			if (isAvailable)
			{
				g_tState.spawnedBarrels++;
				noRoomRetryCounter = 0;
				VectorCopy(groundTrace.endpos, s_barrelPositions[numPositions]);
				numPositions++;
			}
			else
			{
				failDistance++;
				noRoomRetryCounter++;
			}
		}
	} // While end
	Com_Printf("Barrel placement: %d positions found.\n", g_tState.spawnedBarrels);

	// Density: 0 = none, 1-100 = percentage, -1 = auto
	// Auto mode targets ~1 barrel per 200 sq units of map area, capped at entity budget
	{
		int barrelDensity = tm_barrelDensity.integer;
		if (barrelDensity < -1) barrelDensity = -1;
		if (barrelDensity > 100) barrelDensity = 100;

		if (barrelDensity == -1) {
			// Auto: estimate walkable area from map bounds and scale density
			float mapAreaX = g_tState.mapBoundPointsMax[0] - g_tState.mapBoundPointsMin[0];
			float mapAreaY = g_tState.mapBoundPointsMax[1] - g_tState.mapBoundPointsMin[1];
			float mapArea = mapAreaX * mapAreaY;
			// Target: 1 barrel per 430x430 area (~185000 sq units)
			int targetBarrels = (int)(mapArea / 185000.0f);
			if (targetBarrels < 20) targetBarrels = 20;
			if (targetBarrels > numPositions) targetBarrels = numPositions;
			barrelDensity = (numPositions > 0) ? (targetBarrels * 100 / numPositions) : 0;
			if (barrelDensity > 100) barrelDensity = 100;
			if (barrelDensity < 5) barrelDensity = 5;
		}

		// Remove a percentage of barrels if density < 100
		if (barrelDensity != 100)
		{
			int removeNum = (int)(numPositions * (1.0f - barrelDensity / 100.0f));

			while (removeNum > 0 && numPositions > 0)
			{
				int idx = rand() % numPositions;
				VectorCopy(s_barrelPositions[numPositions - 1], s_barrelPositions[idx]);
				numPositions--;
				g_tState.spawnedBarrels--;
				removeNum--;
			}
		}
	}

	s_pendingSpawnCount = numPositions;
	s_pendingSpawnNext = 0;
	// Don't spawn here — Tarascii_ContinueBarrelPlacement will spawn in batches from RunFrame
#endif
}

// Incremental barrel spawning — called from Tarascii_RunFrame each frame.
void Tarascii_ContinueBarrelPlacement(void)
{
	int batchSize = 25;
	int spawned = 0;

	if (s_pendingSpawnNext >= s_pendingSpawnCount) return;

	while (s_pendingSpawnNext < s_pendingSpawnCount && spawned < batchSize)
	{
		Tarascii_SpawnBarrel(s_barrelPositions[s_pendingSpawnNext]);
		s_pendingSpawnNext++;
		spawned++;
	}

	if (s_pendingSpawnNext >= s_pendingSpawnCount)
	{
		s_pendingSpawnCount = 0;
	}
}

void Tarascii_ExplodeBarrel(gentity_t* ent)
{
	if (!g_tState.explodeClicked[ent->s.clientNum])
	{
		g_tState.explodeClicked[ent->s.clientNum] = qtrue;
		g_tState.explodeClick[ent->s.clientNum] = level.time+1000;
		G_Sound( ent, CHAN_WEAPON, G_SoundIndex( "sound/weapons/thermal/warning.wav" ) );
	}
}

void Tarascii_ResetEplodeClick(int clientNum)
{
	g_tState.explodeClicked[clientNum] = qfalse;
}

void Tarascii_ClientThink(gentity_t* ent)
{
	// Kick clients flagged as missing the TarasciiMadness cgame
	if (g_tState.clientVersionTimers[ent->s.clientNum] == -1) {
		g_tState.clientVersionTimers[ent->s.clientNum] = 0;
		trap->DropClient(ent->s.clientNum, "TarasciiMadness client plugin v1.4 required.");
		return;
	}

	// Sets team if allowed to join.
	if ( g_tState.AllowJoin == qtrue && g_tState.startTime != 0 && g_tState.startTime <= level.time && ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		SetTeam(ent, NULL);
	}


	// Checks if the barrel has clicked to explode and if it's time to explode yet.
	if (g_tState.explodeClick[ent->s.clientNum] != 0)
	{
		if (level.time >= g_tState.explodeClick[ent->s.clientNum])
		{
			G_Damage(ent, ent, ent, ent->r.currentAngles, ent->r.currentOrigin, 999, 0, MOD_THERMAL);
			g_tState.explodeClick[ent->s.clientNum] = 0;
		}
		if (ent->health <= 0)
		{
			g_tState.explodeClick[ent->s.clientNum] = 0;
		}
	}

	// Clientplugin turns off zoom completely but this is still needed for clients without the clientplugin.
	ent->client->ps.zoomMode = 0;
}

void Tarascii_BarrelBound(gentity_t* ent, pmove_t* pmove)
{
	if (ent->client->sess.sessionTeam == TEAM_RED)
	{
		// Match the 20x20 box from PM_CheckDuck's barrel override
		vec3_t boxMin = {-10, -10, -24};
		vec3_t boxMax = {10, 10, 26};

		ent->client->ps.crouchheight = 26;
		ent->client->ps.standheight = 26;

		VectorCopy(boxMin, ent->r.mins);
		VectorCopy(boxMax, ent->r.maxs);
	}
}

void Tarascii_ClientSpawn(gentity_t* ent)
{
	int barrelNum = 0;
	vec3_t respawnHeightOffset;

	if ( level.gametype == GT_TEAM )
	{
		// Barrel Player Setup.
		if ( ent->client->sess.sessionTeam == TEAM_RED )
		{
			ent->health = ent->client->ps.stats[STAT_HEALTH] = ent->client->ps.stats[STAT_MAX_HEALTH] * 1.00;
			ent->client->ps.stats[STAT_ARMOR] = 0;
			ent->client->ps.weapon = WP_MELEE;
			ent->client->ps.stats[STAT_WEAPONS] = (1 << WP_MELEE);
			//ent->client->ps.stats[STAT_WEAPONS] = (1 << WP_MELEE | 1 << WP_THERMAL);
			//ent->client->ps.ammo[AMMO_THERMAL] = 999;
			ForceSeeing(ent);

			// Hint for barrel players — only once per round
			if (!g_tState.barrelHintShown[ent->s.clientNum]) {
				g_tState.barrelHintShown[ent->s.clientNum] = qtrue;
				trap->SendServerCommand(ent->s.clientNum, "cp \"^7Hold ^3Right Click ^7to see player names\"");
			}

			// Invisibility
			ent->client->ps.eFlags |= EF_NODRAW;	// Turns off player model.
			ent->client->ps.eFlags &= ~EF_INVULNERABLE; // No spawn protection for barrels
			ent->client->invulnerableTimer = 0;
			ent->client->ps.powerups[PW_CLOAKED] = Q3_INFINITE;	// Turns off mouseover target.

			// If there are no available barrels, forcefully pick the first pending barrel to respawn and respawn it.
			if (g_tState.barrelsToRespawn >= g_tState.spawnedBarrels)
			{
				vec3_t zeroPos = {0,0,0};
				int i;
				for (i = 0; i < MAX_GENTITIES; i++)
				{
					if (VectorCompare(g_tState.respawnBarrelPos[i], zeroPos) == 0)
					{
						int j;
						for (j = 0; j < MAX_CLIENTS; j++)
						{
							if (g_tState.dontRespawn[j] != i)
							{
								Tarascii_SpawnBarrel(g_tState.respawnBarrelPos[i]);
								VectorClear(g_tState.respawnBarrelPos[i]);
								g_tState.barrelsToRespawn--;
								break;
							}
						}
					}
				}
			}

			// Pick a random barrel to give the player.
			while (qtrue)
			{
				barrelNum = random() * (MAX_GENTITIES - (MAX_CLIENTS + BODY_QUEUE_SIZE)) + (MAX_CLIENTS + BODY_QUEUE_SIZE);

				if (Q_stricmp(g_entities[barrelNum].classname,"tarasciiBarrel") == 0)
				{
					int i;
					qboolean foundOccupied = qfalse;
					for (i = 0; i < MAX_CLIENTS; i++)
					{
						if (g_tState.dontRespawn[i] == barrelNum)
						{
							foundOccupied = qtrue;
							break;
						}
					}
					if (!foundOccupied)
					{
						break;
					}
				}
			}

			// Move player to selected barrel (no TeleportPlayer — avoids green lightning effect).
			VectorCopy(g_entities[barrelNum].s.origin,respawnHeightOffset);
			respawnHeightOffset[2] += 23;
			VectorCopy(respawnHeightOffset, ent->client->ps.origin);
			G_SetOrigin(ent, respawnHeightOffset);
			VectorClear(ent->client->ps.velocity);
			SetClientViewAngle( ent, g_entities[barrelNum].s.angles );

			ent->playerState->duelIndex = g_tState.dontRespawn[ent->s.clientNum] = barrelNum;
			g_entities[barrelNum].s.clientNum = ent->s.clientNum+1;
			VectorCopy(g_entities[barrelNum].s.origin, g_tState.respawnBarrelPos[barrelNum]);
			g_tState.barrelsToRespawn++;

			// Make barrel non-solid immediately so weapon traces pass through to hit the player entity
			g_entities[barrelNum].r.contents = 0;
			trap->LinkEntity((sharedEntity_t*)&g_entities[barrelNum]);

			g_entities[barrelNum].s.isPortalEnt = qtrue;
			ent->playerState->duelTime = 1;
		}
		else if (ent->client->sess.sessionTeam == TEAM_BLUE)
		{
			ent->health = ent->client->ps.stats[STAT_HEALTH] = ent->client->ps.stats[STAT_MAX_HEALTH];
			ent->client->ps.stats[STAT_ARMOR] = ent->client->ps.stats[STAT_MAX_HEALTH];
			ent->client->ps.weapon = WP_DISRUPTOR;
			ent->client->ps.stats[STAT_WEAPONS] = (1 << WP_DISRUPTOR);
			ent->client->ps.ammo[AMMO_POWERCELL] = 9999;
			ent->s.isPortalEnt = qtrue;

			// Spread players across spawn points with random offset to avoid stacking
			{
				gentity_t *spot = NULL;
				vec3_t spawnOrigin, spawnAngles;
				// Pick a random spawn point
				int numSpawns = 0;
				gentity_t *spawns[64];
				while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL && numSpawns < 64) {
					spawns[numSpawns++] = spot;
				}
				if (numSpawns > 0) {
					spot = spawns[(int)(random() * numSpawns) % numSpawns];
					VectorCopy(spot->s.origin, spawnOrigin);
					VectorCopy(spot->s.angles, spawnAngles);
					spawnOrigin[2] += 9;
					// Set position directly — no TeleportPlayer to avoid green lightning effect
					VectorCopy(spawnOrigin, ent->client->ps.origin);
					G_SetOrigin(ent, spawnOrigin);
					VectorClear(ent->client->ps.velocity);
					SetClientViewAngle(ent, spawnAngles);
				}
			}
		}
	}
}

void Tarascii_RespawnBarrels()
{
	vec3_t zeroPos = {0,0,0};
	qboolean respawn;

	if (g_tState.barrelsToRespawn > g_tState.barrelTeamCounter)
	{
		int i;
		for (i = 0; i < MAX_GENTITIES; i++)
		{
			if (VectorCompare(g_tState.respawnBarrelPos[i], zeroPos) == 0)
			{
				int j;
				respawn = qtrue;

				for (j = 0; j < MAX_CLIENTS; j++)
				{
					if (g_tState.dontRespawn[j] == i)
					{
						respawn = qfalse;
						break;
					}
				}

				if (respawn)
				{
					Tarascii_SpawnBarrel(g_tState.respawnBarrelPos[i]);
					VectorClear(g_tState.respawnBarrelPos[i]);
					g_tState.barrelsToRespawn--;
				}

				if (g_tState.barrelsToRespawn == g_tState.barrelTeamCounter)
				{
					break;
				}
			}
		}
	}
}

void Tarascii_BarrelNonSolid(int barrelNum)
{
	g_entities[barrelNum].r.contents = 0;
}

void Tarascii_Disconnect(int clientNum)
{
	gentity_t *ent;

	ent = &g_entities[clientNum];

	// Cleanup barrel attachment.
	if (ent->client->sess.sessionTeam == TEAM_RED)
	{
		G_FreeEntity(&g_entities[ent->playerState->duelIndex]);
		g_tState.dontRespawn[clientNum] = 0;
		g_tState.barrelsToRespawn--;
	}

	g_tState.clientVersions[clientNum] = -1;
	g_tState.debugForceTeam[clientNum] = 0;
	g_tState.barrelHintShown[clientNum] = qfalse;
	g_tState.clientVersionTimers[clientNum] = 0;
}

void ExitLevel (void);
void Tarascii_ExitCheck()
{
	// Replays the same map for the set amount of rounds and then moves on to the next level in the queue.
	if (g_tState.sessionNumRounds != tm_rounds.integer)
	{
		g_tState.sessionNumRounds++;
		trap->SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
		return;
	}
	else
	{
		g_tState.sessionNumRounds = 0;
		ExitLevel();
		return;
	}
}

void Tarascii_AddClientPluginVersion(int clientNum, int clientVersion)
{
	g_tState.clientVersions[clientNum] = clientVersion;
}

void Tarascii_ClientBegin(gentity_t* ent)
{
	int clientNum = ent - g_entities;

	// Check for TarasciiMadness cgame via userinfo key set by the cgame
	// Defer the actual drop to ClientThink so the client has received snapshots
	if (!(ent->r.svFlags & SVF_BOT)) {
		char userinfo[MAX_INFO_STRING];
		trap->GetUserinfo(clientNum, userinfo, sizeof(userinfo));
		const char *cgameKey = Info_ValueForKey(userinfo, "tm_cgame");
		if (!cgameKey[0] || Q_stricmp(cgameKey, "1.4") != 0) {
			g_tState.clientVersionTimers[clientNum] = -1; // flag: pending kick
			return;
		}
	}
	g_tState.clientVersionTimers[clientNum] = 0;

	// Sync cvars needed for client-side prediction and HUD
	trap->SendServerCommand(clientNum, va("tmSyncCvar tm_reloadSpeed %d", tm_reloadSpeed.integer));
	trap->SendServerCommand(clientNum, va("tmSyncCvar tm_barrelModelMode %d", tm_barrelModelMode.integer));
}

void Tarascii_PluginIdentify( gentity_t *ent )
{
	if(trap->Argc() == 2)
	{
		char sarg[MAX_STRING_CHARS];
		int clientVersionNumber = -1;

		trap->Argv( 1, sarg, sizeof( sarg ) );

		if(Q_isanumber(sarg))
		{
			clientVersionNumber = atoi(sarg);
			Tarascii_AddClientPluginVersion(ent-g_entities, clientVersionNumber);
		}
	}
}
