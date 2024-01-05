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

ConVar    sk_plr_dmg_iceaxe("sk_plr_dmg_iceaxe", "15");
ConVar    sk_npc_dmg_iceaxe("sk_npc_dmg_iceaxe", "15");

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
	{ ACT_IDLE,				ACT_IDLE_ANGRY_MELEE,	true },
	{ ACT_IDLE_ANGRY,		ACT_IDLE_ANGRY_MELEE,	false },
	{ ACT_VM_HOLSTER,		ACT_VM_HOLSTER,	false },
	{ ACT_VM_MISSCENTER,		ACT_VM_MISSCENTER,	false },
	{ ACT_VM_CHARGE,		ACT_VM_CHARGE,	true },
	{ ACT_VM_CHARGE_MISS,		ACT_VM_CHARGE_MISS,	true },
	{ ACT_VM_CHARGE_IDLE,		ACT_VM_CHARGE_IDLE,	true },
	{ ACT_VM_CHARGE_HIT,		ACT_VM_CHARGE_HIT,	true },
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
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());


	if (pPlayer == NULL)
		return;

	int curActivity = GetActivity();

	//Check for M2
	if (pPlayer && pPlayer->m_afButtonPressed & IN_ATTACK2 && m_flNextPrimaryAttack <= gpGlobals->curtime && m_flNextSecondaryAttack <= gpGlobals->curtime) {
		isCharging = true;
		//Start Charge Duration
		if (chargeDuration == 0.0f) {
			chargeDuration = gpGlobals->curtime;
		}
	}
	//Stop Charge
	else if (pPlayer && pPlayer->m_afButtonReleased & IN_ATTACK2){
		isCharging = false;
	}

	//Swing
	if (gpGlobals->curtime >= chargeDuration + 1.0f && !isCharging && chargeDuration != 0.0f) {
		addedDamage = (gpGlobals->curtime - chargeDuration)*10.0f;

		if (addedDamage >= 65.0f)
		{
			Msg("%f\n", addedDamage);
			addedDamage = 65.0f;
		}
		AddViewKick();
		BaseClass::SecondaryAttack();
		TraceMeleeAttack();
		
		chargeDuration = 0.0f;
		addedDamage = 0.0f;
	}

	//Animate the Charging
	if (chargeDuration != 0.0f) {
		if (GetActivity() != ACT_VM_CHARGE) {
			SendWeaponAnim(ACT_VM_CHARGE);
			Msg("%i\n", curActivity);
		}
	}

#ifdef MAPBASE
	if (pPlayer->HasSpawnFlags(SF_PLAYER_SUPPRESS_FIRING))
	{
		BaseClass::WeaponIdle();
		return;
	}
#endif

	if ((pPlayer->m_nButtons & IN_ATTACK) && (m_flNextPrimaryAttack <= gpGlobals->curtime))
	{
		chargeDuration = 0.0f;
		addedDamage = 0.0f;
		BaseClass::PrimaryAttack();
	}
	else if (chargeDuration == 0.0f)
	{
		BaseClass::WeaponIdle();
		return;
	}
}

void CWeaponIceaxe::TraceMeleeAttack()
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
	if (!pPlayer)
		return;

	Vector swingStart = pPlayer->Weapon_ShootPosition();
	Vector forward;

	AngleVectors(pPlayer->EyeAngles(), &forward);

	Vector swingEnd = swingStart + forward * GetRange(); //Get range of attack

	trace_t traceHit;
	UTIL_TraceLine(swingStart, swingEnd, MASK_SHOT_HULL, pPlayer, COLLISION_GROUP_NONE, &traceHit);

	if (traceHit.m_pEnt)
	{
		// Hit
		SendWeaponAnim(ACT_VM_CHARGE_HIT);
	}
	else
	{
		// Miss
		// If an enemy dies it will always play this
		SendWeaponAnim(ACT_VM_CHARGE_MISS);
	}
	addedDamage = 0;
}