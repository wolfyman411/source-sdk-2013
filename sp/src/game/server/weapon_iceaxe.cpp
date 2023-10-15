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
		Msg("%f\n", sk_plr_dmg_iceaxe.GetFloat() + addedDamage);
		return sk_plr_dmg_iceaxe.GetFloat();
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

	addedDamage = 0;
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

bool cooldown = false;
float chargeDuration = 0.0f;
float chargeStart = 0.0f;
float lastSwing = 0.0f;
bool isCharging = false;
bool canSwing = true;
bool startBuildup = false;
void CWeaponIceaxe::ItemPostFrame()
{
	// Call the base class function first
	BaseClass::ItemPostFrame();
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	//Check for m2
	if (pPlayer && pPlayer->m_afButtonPressed & IN_ATTACK2 && cooldown == false)
	{
		SendWeaponAnim(ACT_VM_CHARGE);
		isCharging = true;
		canSwing = false;
		//Check if new swing
		if (chargeDuration == 0.0f)
		{
			Msg("StartCharge!\n");
			chargeStart = gpGlobals->curtime;
			startBuildup = true;
		}
	}
	else if (pPlayer && pPlayer->m_afButtonReleased & IN_ATTACK2 && cooldown == false)
	{
		canSwing = true;
	}

	//Get Charge
	if (isCharging)
	{
		chargeDuration = gpGlobals->curtime - chargeStart;
		//Swing
		if (canSwing && chargeDuration > 1.0f)
		{
			isCharging = false;
			canSwing = false;
			AddViewKick();
			BaseClass::SecondaryAttack();
			TraceMeleeAttack();

			//added damage 
			if (chargeDuration < 1.4f)
			{
				addedDamage = 15;
			}
			else if (chargeDuration > 1.4f && chargeDuration < 2.0f)
			{
				addedDamage = 25;
			}
			else if (chargeDuration > 2.0f)
			{
				addedDamage = 40;
			}

			lastSwing = gpGlobals->curtime;
			chargeDuration = 0.0f;
			cooldown = true;
		}
	}

	if (gpGlobals->curtime - lastSwing > 1.8f)
	{
		cooldown = false;
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