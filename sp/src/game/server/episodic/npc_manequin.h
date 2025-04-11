/*
A manequin NPC that does not move when the player can see it
*/

#ifndef NPC_MANEQUIN
#define NPC_MANEQUIN
#pragma once

#include "ai_basenpc.h"
#include "hl2_gamerules.h"

class CNPC_Manequin : public CAI_BaseNPC
{
	DECLARE_CLASS( CNPC_Manequin, CAI_BaseNPC );

public:
	void Spawn( void );
	void Precache( void );
	void Think( void );
	void InputStopAttack( inputdata_t& inputdata );
	void InputStartAttack( inputdata_t& inputdata );

	int MeleeAttack1Conditions( float flDot, float flDist );

	virtual bool Can_Move( void );
	Class_T Classify( void ) { return CLASS_MANEQUIN; } // No class, no squad

	

	DECLARE_DATADESC();
private:
	bool m_bShouldAttack;
};

#endif