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
#define	ICEAXE_REFIRE	0.7f

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
	float		GetDamageForActivity(Activity hitActivity);

	virtual int WeaponMeleeAttack1Condition(float flDot, float flDist);
	void	SecondaryAttack() { return; }
	void	TraceMeleeAttack();
	const char* text;

private:
	// Animation event handlers
	bool isCharging = false;
	bool canSwing = false;
	float chargeDuration;
	float addedDamage = 0.0f;
};

#endif // WEAPON_ICEAXE_H
