//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "Color.h"

#ifndef COMBINE_MINE_H
#define COMBINE_MINE_H

#ifdef _WIN32
#pragma once
#endif

class CSoundPatch;

//---------------------------------------------------------
//---------------------------------------------------------
#define BOUNCEBOMB_HOOK_RANGE		64
#define BOUNCEBOMB_WARN_RADIUS		245.0	// Must be slightly less than physcannon!
#define BOUNCEBOMB_DETONATE_RADIUS	100.0

#define BOUNCEBOMB_EXPLODE_RADIUS	125.0
#define BOUNCEBOMB_EXPLODE_DAMAGE	150.0
#include "player_pickup.h"
#ifdef MAPBASE
#include "filters.h"
#endif

class CBounceBomb : public CBaseAnimating, public CDefaultPlayerPickupVPhysics
{
	DECLARE_CLASS( CBounceBomb, CBaseAnimating );

public:
#ifdef MAPBASE
	CBounceBomb() { m_pWarnSound = NULL; m_bPlacedByPlayer = false; m_flExplosionDelay = 0.5f; m_iLOSMask = MASK_SOLID_BRUSHONLY; m_vecPlantOrientation = vec3_invalid; }
#else
	CBounceBomb() { m_pWarnSound = NULL; m_bPlacedByPlayer = false; }
#endif
	void Precache();
	void Spawn();
	void OnRestore();
	int DrawDebugTextOverlays(void);
	void SetMineState( int iState );
	int GetMineState() { return m_iMineState; }
	bool IsValidLocation();
	void Flip( const Vector &vecForce, const AngularImpulse &torque );
	void SearchThink();
	void BounceThink();
	void SettleThink();
	void CaptiveThink();
	void ExplodeThink();
	void ExplodeTouch( CBaseEntity *pOther );
	void CavernBounceThink(); ///< an alternative style of bouncing used for the citizen modded bouncers
	bool IsAwake() { return m_bAwake; }
	void Wake( bool bWake );
	float FindNearestNPC();
	void SetNearestNPC( CBaseEntity *pNearest ) { m_hNearestNPC.Set( pNearest ); }
	int OnTakeDamage( const CTakeDamageInfo &info );
	bool IsFriend( CBaseEntity *pEntity );

	void UpdateLight( bool bTurnOn, unsigned int r, unsigned int g, unsigned int b, unsigned int a );
	bool IsLightOn() { return m_hSprite.Get() != NULL; }

	void OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason = PICKED_UP_BY_CANNON );
	void OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason );
	bool ForcePhysgunOpen( CBasePlayer *pPlayer ) { return true; }
	bool HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer ) { return true; }
	virtual QAngle	PreferredCarryAngles( void ) { return vec3_angle; }
	CBasePlayer *HasPhysicsAttacker( float dt );

	bool IsPlayerPlaced() { return m_bPlacedByPlayer; }

	// Determines whether companions should treat the mine as a navigation obstacle and avoid it
	bool ShouldBeAvoidedByCompanions();

	bool CreateVPhysics()
	{
		VPhysicsInitNormal( SOLID_VPHYSICS, 0, false );
		return true;
	}

	void Pickup();

	void OpenHooks( bool bSilent = false );
	void CloseHooks();

#ifdef MAPBASE
	// Uses the new CBaseEntity interaction implementation and replaces the dynamic_casting from npc_barnacle
	bool	HandleInteraction( int interactionType, void *data, CBaseCombatCharacter* sourceEnt );

	void	UpdateWarnSound( float flVolume, float flDelta );
	void	SilenceWarnSound( float flDelta );
#endif

	DECLARE_DATADESC();

	static string_t gm_iszFloorTurretClassname;
	static string_t gm_iszGroundTurretClassname;

private:
	float		m_flExplosionDelay;

	bool	m_bAwake;
	bool	m_bBounce;
	EHANDLE	m_hNearestNPC;
	EHANDLE m_hSprite;
	Color 	m_LastSpriteColor;

	float	m_flHookPositions;
	int		m_iHookN;
	int		m_iHookE;
	int		m_iHookS;
	int		m_iAllHooks;

	CSoundPatch	*m_pWarnSound;

	bool	m_bLockSilently;
	bool	m_bFoeNearest;

	float	m_flIgnoreWorldTime;

	bool	m_bDisarmed;
#ifdef MAPBASE
	int		m_iInitialState;
	bool	m_bCheapWarnSound;

	// Allows control over the mask used in LOS
	int		m_iLOSMask;

	bool	m_bUnavoidable;

	// What direction the mine should be facing when planting itself (i.e. facing up, facing left, etc.)
	// vec3_invalid = use default (0 0 1 or -90 0 0)
	Vector	m_vecPlantOrientation;
#endif

	bool	m_bPlacedByPlayer;

	bool	m_bHeldByPhysgun;

	int		m_iFlipAttempts;
	int     m_iModification;

	CHandle<CBasePlayer>	m_hPhysicsAttacker;
	float					m_flLastPhysicsInfluenceTime;

	float					m_alteredMass;

	float					m_flTimeGrabbed;
	IPhysicsConstraint		*m_pConstraint;
	int						m_iMineState;

#ifdef MAPBASE
	// Makes the filters the exclusive factor in determining friend/foe
	bool		m_bFilterExclusive;

	string_t	m_iszEnemyFilter;
	CHandle<CBaseFilter>	m_hEnemyFilter;
	void InputSetEnemyFilter( inputdata_t &inputdata );

	string_t	m_iszFriendFilter;
	CHandle<CBaseFilter>	m_hFriendFilter;
	void InputSetFriendFilter( inputdata_t &inputdata );
#endif

	COutputEvent	m_OnPulledUp;
	void InputDisarm( inputdata_t &inputdata );
#ifdef MAPBASE
	void InputBounce( inputdata_t &inputdata );
	void InputBounceAtTarget( inputdata_t &inputdata );
	void InputSetPlantOrientation( inputdata_t &inputdata );
	void InputSetPlantOrientationRaw( inputdata_t &inputdata );
	COutputEvent	m_OnTriggered;
	COutputEvent	m_OnExplode;
#endif
};



#endif // COMBINE_MINE_H
