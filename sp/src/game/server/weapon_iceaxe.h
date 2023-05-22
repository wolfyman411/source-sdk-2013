//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef WEAPON_ICEAXE_H
#define WEAPON_ICEAXE_H

#include "basebludgeonweapon.h"

#if defined( _WIN32 )
#pragma once
#endif

#ifdef HL2MP
#error weapon_iceaxe.h must not be included in hl2mp. The windows compiler will use the wrong class elsewhere if it is.
#endif

#define	ICEAXE_RANGE	75.0f
#define	ICEAXE_REFIRE	0.6f

//-----------------------------------------------------------------------------
// CWeaponIceaxe
//-----------------------------------------------------------------------------

class CWeaponIceaxe : public CBaseHLBludgeonWeapon
{
public:
	DECLARE_CLASS(CWeaponIceaxe, CBaseHLBludgeonWeapon);
	DECLARE_SERVERCLASS();
	DECLARE_ACTTABLE();

	CWeaponIceaxe();

	float		GetRange(void) { return	ICEAXE_RANGE; }
	float		GetFireRate(void) { return	ICEAXE_REFIRE; }

	void		AddViewKick(void);
	void		ItemPostFrame();
	//void		PerformSecondaryAttack();
	//void		ResetSecondaryAttackCharge();
	float		GetDamageForActivity(Activity hitActivity);

	virtual int WeaponMeleeAttack1Condition(float flDot, float flDist);
	//void SecondaryAttack(float flDot, float flDist);
	//virtual int WeaponMeleeAttack2Condition(float flDot, float flDist);
	void	SecondaryAttack() { return; }

	// Animation event
	virtual void Operator_HandleAnimEvent(animevent_t* pEvent, CBaseCombatCharacter* pOperator);

	const char* text;

private:
	// Animation event handlers
	void HandleAnimEventMeleeHit(animevent_t* pEvent, CBaseCombatCharacter* pOperator);
	bool isCharging = false;
	bool canSwing = false;
	float chargeStart;
};

#endif // WEAPON_ICEAXE_H
