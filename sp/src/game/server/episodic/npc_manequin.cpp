#include "cbase.h"
#include "ai_basenpc.h"
#include "gameinterface.h"
#include "ai_navigator.h"
#include "player.h"

class CNPC_Mannequin : public CAI_BaseNPC {
public:
	DECLARE_CLASS( CNPC_Mannequin, CAI_BaseNPC );
	DECLARE_DATADESC();

	void Spawn();
	void Precache();
	void Think();
	int MeleeAttack1Conditions( float flDot, float flDist );

	Class_T Classify( void ) { return CLASS_MANEQUIN; } // No specific classification
private:
	bool IsLookedAt( CBasePlayer* pPlayer );
};

LINK_ENTITY_TO_CLASS( npc_manequin, CNPC_Mannequin );

BEGIN_DATADESC( CNPC_Mannequin )
	DEFINE_THINKFUNC(Think),
END_DATADESC()

void CNPC_Mannequin::Spawn() {
	BaseClass::Spawn();
	Precache();
	SetModel( "models/props_junk/gnome.mdl" );

	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_STEP );

	CapabilitiesAdd( bits_CAP_MOVE_GROUND );

	ConColorMsg( Color(0, 255, 155), "Mannequin spawned.\n" );

	SetThink( &CNPC_Mannequin::Think );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CNPC_Mannequin::Precache() {
	PrecacheModel( "models/props_junk/gnome.mdl" );
	BaseClass::Precache();
}

void CNPC_Mannequin::Think() {
	CBasePlayer* pPlayer = UTIL_GetLocalPlayer();
	if ( pPlayer && IsLookedAt( pPlayer ) ) {
		Msg("Player is looking at the mannequin.\n" );

		Vector vecToTarget = GetAbsOrigin() - pPlayer->EyePosition();
		float flDistance = vecToTarget.Length();
		Msg("Distance to player: %f\n", flDistance );

		if ( flDistance < 100.0f ) {
			AI_NavGoal_t goal( pPlayer->GetAbsOrigin() );
			GetNavigator()->SetGoal( goal );
		} else {
			GetNavigator()->StopMoving();
		}
	}
	else {
		GetNavigator()->StopMoving();
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}

bool CNPC_Mannequin::IsLookedAt( CBasePlayer* pPlayer ) {
	Vector forward;
	pPlayer->EyeVectors( &forward );
	Vector vecToTarget = GetAbsOrigin() - pPlayer->EyePosition();
	float flDot = forward.Dot( vecToTarget.Normalized() );
	Msg("Dot product: %f\n", flDot );

	return ( flDot > 0.9f ); // Adjust the threshold as needed
}