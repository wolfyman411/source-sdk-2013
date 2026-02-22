//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GRENADE_TELEPORT_H
#define GRENADE_TELEPORT_H
#pragma once

class CBaseGrenade;
struct edict_t;

CBaseGrenade* Teleportgrenade_Create( const Vector& position, const QAngle& angles, const Vector& velocity, const AngularImpulse& angVelocity, CBaseEntity* pOwner, float timer, bool combineSpawned );
bool	Teleportgrenade_WasPunted( const CBaseEntity* pEntity );
bool	Teleportgrenade_WasCreatedByCombine( const CBaseEntity* pEntity );

#endif // GRENADE_FRAG_H
