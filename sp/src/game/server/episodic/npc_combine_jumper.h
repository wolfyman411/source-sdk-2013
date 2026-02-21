#ifndef NPC_COMBINES_JUMPER_H
#define NPC_COMBINES_JUMPER_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "npc_combines.h"
#include "npc_combine_armored.h"
#include "props.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CNPC_Combine_Jumper : public CNPC_CombineS
{
    DECLARE_CLASS( CNPC_Combine_Jumper, CNPC_CombineS );

public:
    void Spawn( void );
    void Precache( void );

    bool IsJumpLegal( const Vector& startPos, const Vector& apex, const Vector& endPos ) const;

    void Event_Killed( const CTakeDamageInfo& info );

    CNPC_Combine_Armored* m_pSpawnedBy;
};

LINK_ENTITY_TO_CLASS( npc_combine_jumper, CNPC_Combine_Jumper );

#endif // NPC_COMBINES_ARMOURED_H

#ifndef GRENADE_TELEPORT_H
#define GRENADE_TELEPORT_H
#ifdef _WIN32
#pragma once
#endif

class CBaseGrenade;
struct edict_t;

CBaseGrenade* Teleportgrenade_Create( const Vector& position, const QAngle& angles, const Vector& velocity, const AngularImpulse& angVelocity, CBaseEntity* pOwner, float timer, bool combineSpawned );
bool	Teleportgrenade_WasPunted( const CBaseEntity* pEntity );

#endif // GRENADE_TELEPORT_H