//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Iceaxe - an old favorite
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "basehlcombatweapon.h"
#include "player.h"
#include "gamerules.h"
#include "ammodef.h"
#include "mathlib/mathlib.h"
#include "in_buttons.h"
#include "soundent.h"
#include "basebludgeonweapon.h"
#include "vstdlib/random.h"
#include "npcevent.h"
#include "ai_basenpc.h"
#include "weapon_iceaxe.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar    sk_plr_dmg_iceaxe("sk_plr_dmg_iceaxe", "10");
ConVar    sk_npc_dmg_iceaxe("sk_npc_dmg_iceaxe", "10");

//-----------------------------------------------------------------------------
// CWeaponIceaxe
//-----------------------------------------------------------------------------

IMPLEMENT_SERVERCLASS_ST(CWeaponIceaxe, DT_WeaponIceaxe)
END_SEND_TABLE()

#ifndef HL2MP
LINK_ENTITY_TO_CLASS(weapon_iceaxe, CWeaponIceaxe);
PRECACHE_WEAPON_REGISTER(weapon_iceaxe);
#endif

acttable_t CWeaponIceaxe::m_acttable[] =
{
	{ ACT_MELEE_ATTACK1,	ACT_MELEE_ATTACK_SWING, true },
	{ ACT_IDLE,				ACT_IDLE_ANGRY_MELEE,	false },
	{ ACT_IDLE_ANGRY,		ACT_IDLE_ANGRY_MELEE,	false },
	{ ACT_VM_HOLSTER,		ACT_VM_HOLSTER,	false },
	{ ACT_VM_MISSCENTER,		ACT_VM_MISSCENTER,	false },
};

IMPLEMENT_ACTTABLE(CWeaponIceaxe);

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CWeaponIceaxe::CWeaponIceaxe(void)
{
}

//-----------------------------------------------------------------------------
// Purpose: Get the damage amount for the animation we're doing
// Input  : hitActivity - currently played activity
// Output : Damage amount
//-----------------------------------------------------------------------------
float addedDamage = 0.0f;
float CWeaponIceaxe::GetDamageForActivity(Activity hitActivity)
{
	if ((GetOwner() != NULL) && (GetOwner()->IsPlayer()))
	{
		return sk_plr_dmg_iceaxe.GetFloat() + addedDamage;
	}

	return sk_npc_dmg_iceaxe.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Add in a view kick for this weapon
//-----------------------------------------------------------------------------
void CWeaponIceaxe::AddViewKick(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (pPlayer == NULL)
		return;

	QAngle punchAng;

	punchAng.x = random->RandomFloat(1.0f, 2.0f);
	punchAng.y = random->RandomFloat(-2.0f, -1.0f);
	punchAng.z = 0.0f;

	pPlayer->ViewPunch(punchAng);
}


//-----------------------------------------------------------------------------
// Attempt to lead the target (needed because citizens can't hit manhacks with the iceaxe!)
//-----------------------------------------------------------------------------
ConVar sk_iceaxe_lead_time("sk_iceaxe_lead_time", "0.9");

int CWeaponIceaxe::WeaponMeleeAttack1Condition(float flDot, float flDist)
{
	// Attempt to lead the target (needed because citizens can't hit manhacks with the iceaxe!)
	CAI_BaseNPC* pNPC = GetOwner()->MyNPCPointer();
	CBaseEntity* pEnemy = pNPC->GetEnemy();
	if (!pEnemy)
		return COND_NONE;

	Vector vecVelocity;
	vecVelocity = pEnemy->GetSmoothedVelocity();

	// Project where the enemy will be in a little while
	float dt = sk_iceaxe_lead_time.GetFloat();
	dt += random->RandomFloat(-0.3f, 0.2f);
	if (dt < 0.0f)
		dt = 0.0f;

	Vector vecExtrapolatedPos;
	VectorMA(pEnemy->WorldSpaceCenter(), dt, vecVelocity, vecExtrapolatedPos);

	Vector vecDelta;
	VectorSubtract(vecExtrapolatedPos, pNPC->WorldSpaceCenter(), vecDelta);

	if (fabs(vecDelta.z) > 70)
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	Vector vecForward = pNPC->BodyDirection2D();
	vecDelta.z = 0.0f;
	float flExtrapolatedDist = Vector2DNormalize(vecDelta.AsVector2D());
	if ((flDist > 64) && (flExtrapolatedDist > 64))
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	float flExtrapolatedDot = DotProduct2D(vecDelta.AsVector2D(), vecForward.AsVector2D());
	if ((flDot < 0.7) && (flExtrapolatedDot < 0.7))
	{
		return COND_NOT_FACING_ATTACK;
	}

	return COND_CAN_MELEE_ATTACK1;
}

void CWeaponIceaxe::ItemPostFrame()
{
	// Call the base class function first
	BaseClass::ItemPostFrame();
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
	if (pPlayer && pPlayer->m_nButtons & IN_ATTACK2)
	{
		AddViewKick();
		addedDamage = 0.0f;
	}

	if (pPlayer && pPlayer->m_afButtonPressed & IN_ATTACK2)
	{
		if (chargeStart == gpGlobals->curtime || chargeStart == 0.0f)
		{
			chargeStart = gpGlobals->curtime;
			isCharging = true;
		}
	}
	else if (pPlayer && pPlayer->m_afButtonReleased & IN_ATTACK2)
	{
		isCharging = false;
		canSwing = true;
	}

	if (gpGlobals->curtime - chargeStart > 1.0f && isCharging == false && !(pPlayer->m_nButtons & IN_ATTACK2) && canSwing == true)
	{
		if (gpGlobals->curtime - chargeStart > 1.0f && gpGlobals->curtime - chargeStart < 1.5)
		{
			addedDamage = 10.0f;
		}
		else if (gpGlobals->curtime - chargeStart > 1.5f && gpGlobals->curtime - chargeStart < 2.0)
		{
			addedDamage = 15.0f;
		}
		else if (gpGlobals->curtime - chargeStart > 2.0f && gpGlobals->curtime - chargeStart < 3.0)
		{
			addedDamage = 30.0f;
		}
		else if (gpGlobals->curtime - chargeStart > 3.0f)
		{
			addedDamage = 50.0f;
		}
		AddViewKick();
		BaseClass::SecondaryAttack();
		chargeStart = gpGlobals->curtime;
		canSwing = false;
		addedDamage = 0.0f;
		SendWeaponAnim(ACT_VM_MISSCENTER);
	}

	//Anim Handler
	if (pPlayer && pPlayer->m_afButtonPressed & IN_ATTACK2 || isCharging)
	{
		SendWeaponAnim(ACT_VM_HOLSTER);
	}
}

//-----------------------------------------------------------------------------
// Animation event handlers
//-----------------------------------------------------------------------------
void CWeaponIceaxe::HandleAnimEventMeleeHit(animevent_t* pEvent, CBaseCombatCharacter* pOperator)
{
	// Trace up or down based on where the enemy is...
	// But only if we're basically facing that direction
	Vector vecDirection;
	AngleVectors(GetAbsAngles(), &vecDirection);

	CBaseEntity* pEnemy = pOperator->MyNPCPointer() ? pOperator->MyNPCPointer()->GetEnemy() : NULL;
	if (pEnemy)
	{
		Vector vecDelta;
		VectorSubtract(pEnemy->WorldSpaceCenter(), pOperator->Weapon_ShootPosition(), vecDelta);
		VectorNormalize(vecDelta);

		Vector2D vecDelta2D = vecDelta.AsVector2D();
		Vector2DNormalize(vecDelta2D);
		if (DotProduct2D(vecDelta2D, vecDirection.AsVector2D()) > 0.8f)
		{
			vecDirection = vecDelta;
		}
	}

	Vector vecEnd;
	VectorMA(pOperator->Weapon_ShootPosition(), 50, vecDirection, vecEnd);
	CBaseEntity* pHurt = pOperator->CheckTraceHullAttack(pOperator->Weapon_ShootPosition(), vecEnd,
		Vector(-16, -16, -16), Vector(36, 36, 36), sk_npc_dmg_iceaxe.GetFloat(), DMG_CLUB, 0.75);

	// did I hit someone?
	if (pHurt)
	{
		// play sound
		WeaponSound(MELEE_HIT);

		// Fake a trace impact, so the effects work out like a player's crowbaw
		trace_t traceHit;
		UTIL_TraceLine(pOperator->Weapon_ShootPosition(), pHurt->GetAbsOrigin(), MASK_SHOT_HULL, pOperator, COLLISION_GROUP_NONE, &traceHit);
		ImpactEffect(traceHit);
	}
	else
	{
		WeaponSound(MELEE_MISS);
	}
}


//-----------------------------------------------------------------------------
// Animation event
//-----------------------------------------------------------------------------
void CWeaponIceaxe::Operator_HandleAnimEvent(animevent_t* pEvent, CBaseCombatCharacter* pOperator)
{
	switch (pEvent->event)
	{
	case EVENT_WEAPON_MELEE_HIT:
		HandleAnimEventMeleeHit(pEvent, pOperator);
		break;

	default:
		BaseClass::Operator_HandleAnimEvent(pEvent, pOperator);
		break;
	}
}