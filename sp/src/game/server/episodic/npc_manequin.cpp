/*
A manequin NPC that does not move when the player can see it
*/

#include "cbase.h"
#include "game.h"
#include "AI_Default.h"
#include "AI_Schedule.h"
#include "AI_Hull.h"
#include "AI_Navigator.h"
#include "AI_Route.h"
#include "AI_Squad.h"
#include "AI_SquadSlot.h"
#include "AI_Hint.h"
#include "NPCEvent.h"
#include "animation.h"
#include "npc_manequin.h"
#include "gib.h"
#include "soundent.h"
#include "ndebugoverlay.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "movevars_shared.h"
#include "AI_Senses.h"
#include "animation.h"
#include "util.h"
#include "shake.h"
#include "movevars_shared.h"
#include "hl2_shareddefs.h"
#include "particle_parse.h"

//ConVar sk_manequin_health( "sk_manequin_health", "35" ); // bloodycop: Not sure we need that for manequins.
ConVar sk_manequin_dmg_blast( "sk_manequin_dmg", "15" );

BEGIN_DATADESC(CNPC_Manequin)
	DEFINE_FIELD( m_bShouldAttack, FIELD_BOOLEAN ),

	DEFINE_INPUTFUNC( FIELD_VOID, "StartAttack", InputStartAttack ),
	DEFINE_INPUTFUNC( FIELD_VOID, "StopAttack", InputStopAttack ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_manequin, CNPC_Manequin );

void CNPC_Manequin::Spawn( void ) {
	Precache();
	SetRenderColor( 255, 255, 255 );

	SetModel( "models/props_junk/gnome.mdl" );
	SetHullType( HULL_SMALL );

	SetSolid( SOLID_BBOX );
	SetMoveType( MOVETYPE_STEP );

	CapabilitiesClear();
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_SQUAD );

	SetBloodColor( DONT_BLEED );
}

void CNPC_Manequin::Precache() {
	PrecacheModel( "models/props_junk/gnome.mdl" );
	BaseClass::Precache();
}

int CNPC_Manequin::MeleeAttack1Conditions( float flDot, float flDist ) {
	if ( flDist > 64 ) {
		return COND_TOO_FAR_TO_ATTACK;
	}

	if ( flDot < 0.5 ) {
		return COND_NOT_FACING_ATTACK;
	}

	return COND_CAN_MELEE_ATTACK1;
}

void CNPC_Manequin::Think() {
	if ( m_bShouldAttack ) {
		SetSchedule( SCHED_MELEE_ATTACK1 );
	} else {
		SetSchedule( SCHED_IDLE_STAND );
	}
	BaseClass::Think();
}

bool CNPC_Manequin::Can_Move( void ) {
	return m_bShouldAttack;
}

void CNPC_Manequin::InputStartAttack( inputdata_t& inputdata ) {
	m_bShouldAttack = true;
}

void CNPC_Manequin::InputStopAttack( inputdata_t& inputdata ) {
	m_bShouldAttack = false;
}