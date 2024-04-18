//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "EntityList.h"
#include "engine/IEngineSound.h"
#include "soundent.h"
#include "globalstate.h"
#include "soundscape.h"
#include "sprite.h"
#include "actanimating.h"
#include "eventqueue.h"
#include "ai_baseactor.h"
#include "ai_behavior.h"
#include "baseentity.h"
#include "util.h"
#include <time.h> 
#include "player.h"
#include "isaverestore.h"

#define XEN_PLANT_GLOW_SPRITE		"sprites/light_glow03.vmt"
#define XEN_PLANT_HIDE_TIME			5
#define SKIN_DEFAULT				0
#define SKIN_DIM					1

class CXenPLight : public CAI_BaseActor
{
	DECLARE_CLASS(CXenPLight, CAI_BaseActor);

public:

	DECLARE_DATADESC();

	void		Spawn(void);
	void		Precache(void);
	void		Touch(CAI_BaseActor* pOther);
	void		Think(void);

	void		LightOn(void);
	void		LightOff(void);

	float		m_flDmgTime;
	Class_T			Classify(void) { return CLASS_EARTH_FAUNA; }

protected:
	COutputEvent m_OnHide;
	COutputEvent m_OnShow;

private:
	CBroadcastRecipientFilter filter;
	float	transRatio;
	CSprite* m_pGlow;
	CBasePlayer* pPlayer;
	CBaseEntity* pLightEntity;
};

LINK_ENTITY_TO_CLASS(xen_plantlight, CXenPLight);

BEGIN_DATADESC(CXenPLight)

DEFINE_FIELD(m_pGlow, FIELD_CLASSPTR),
DEFINE_FIELD(m_flDmgTime, FIELD_FLOAT),
DEFINE_FIELD(pLightEntity, FIELD_CLASSPTR),

DEFINE_OUTPUT(m_OnHide, "OnHide"),
DEFINE_OUTPUT(m_OnShow, "OnShow"),

END_DATADESC()

void CXenPLight::Spawn(void)
{
	Precache();

	SetModel("models/model_xen/xen_stalk.mdl");

	SetMoveType(MOVETYPE_NONE);
	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_TRIGGER | FSOLID_NOT_SOLID);

	UTIL_SetSize(this, Vector(-80, -80, 0), Vector(80, 80, 32));
	SetActivity(ACT_IDLE);
	SetNextThink(gpGlobals->curtime + 0.1);
	SetCycle(random->RandomFloat(0, 1));

	m_pGlow = CSprite::SpriteCreate(XEN_PLANT_GLOW_SPRITE, GetLocalOrigin() + Vector(0, 0, (WorldAlignMins().z + WorldAlignMaxs().z) * 0.1), FALSE);
	m_pGlow->SetTransparency(kRenderGlow, GetRenderColor().r, GetRenderColor().g, GetRenderColor().b, GetRenderColor().a*0.5f, m_nRenderFX);
	m_pGlow->SetAttachment(this, 1);
	transRatio = 1;

	LightOn();
}


void CXenPLight::Precache(void)
{
	PrecacheModel("models/model_xen/xen_stalk.mdl");
	PrecacheModel(XEN_PLANT_GLOW_SPRITE);
	SetActivity(ACT_IDLE);

	// Create the dynamic light entity if it doesn't exist
	if (!pLightEntity)
	{
		pLightEntity = CreateEntityByName("light_dynamic");
		pLightEntity->SetAbsOrigin(GetAbsOrigin() + Vector(0, 0, 50));
		pLightEntity->Activate();
		DispatchSpawn(pLightEntity);
	}
}

void CXenPLight::Think(void)
{
	StudioFrameAdvance();
	SetNextThink(gpGlobals->curtime + 0.1);
	CBasePlayer* pPlayer = AI_GetSinglePlayer();

	//Get player Distance
	float playerDis = (pPlayer->GetAbsOrigin() - this->GetAbsOrigin()).Length();

	//Set Transparency so you don't blind the player
	if (playerDis < 1000.0f)
	{
		transRatio = (1000.0f - playerDis) / 1000.0f;
		m_pGlow->SetTransparency(kRenderGlow, GetRenderColor().r, GetRenderColor().g, GetRenderColor().b, GetRenderColor().a * transRatio, m_nRenderFX);
	}
	else
	{
		m_pGlow->SetTransparency(kRenderGlow, GetRenderColor().r, GetRenderColor().g, GetRenderColor().b, GetRenderColor().a * 0.0f, m_nRenderFX);
	}

	switch (GetActivity())
	{
	case ACT_CROUCH:
		if (IsSequenceFinished())
		{
			SetActivity(ACT_CROUCHIDLE);
			LightOff();
		}
		break;

	case ACT_CROUCHIDLE:
		//See how close the player is
		if (playerDis > 300.0f) {
			SetActivity(ACT_STAND);
			LightOn();
		}
		break;

	case ACT_STAND:
		if (IsSequenceFinished())
			SetActivity(ACT_IDLE);
		break;

	case ACT_IDLE:
		//See how close the player is
		if (playerDis < 200.0f) {
			SetActivity(ACT_CROUCH);
		}
		break;
	default:
		break;
	}
}


void CXenPLight::Touch(CAI_BaseActor* pOther)
{
	if (pOther->IsPlayer())
	{
		m_flDmgTime = gpGlobals->curtime + XEN_PLANT_HIDE_TIME;
		if (GetActivity() == ACT_IDLE || GetActivity() == ACT_STAND)
		{
			SetActivity(ACT_CROUCH);
		}
	}
}


void CXenPLight::LightOn(void)
{
	variant_t Value;
	g_EventQueue.AddEvent(STRING(m_target), "TurnOn", Value, 0, this, this);
	m_OnShow.FireOutput(NULL, this);

	if (m_pGlow)
	{
		m_pGlow->RemoveEffects(EF_NODRAW);
		m_nSkin = SKIN_DEFAULT;

		pLightEntity->KeyValue("_light", UTIL_VarArgs("%d %d %d", 255, 240, 111));
		pLightEntity->KeyValue("distance", UTIL_VarArgs("%f", 300.0f));
		pLightEntity->KeyValue("brightness", UTIL_VarArgs("%f", 0.1f));
		DispatchSpawn(pLightEntity);
	}
}


void CXenPLight::LightOff(void)
{
	variant_t Value;
	g_EventQueue.AddEvent(STRING(m_target), "TurnOff", Value, 0, this, this);
	m_OnHide.FireOutput(NULL, this);

	if (m_pGlow)
	{
		
		m_pGlow->AddEffects(EF_NODRAW);
		m_nSkin = SKIN_DIM;

		pLightEntity->KeyValue("_light", UTIL_VarArgs("%d %d %d", 0, 0, 0));
		pLightEntity->KeyValue("distance", UTIL_VarArgs("%f", 0.0f));
		pLightEntity->KeyValue("brightness", UTIL_VarArgs("%f", 0.0f));
		DispatchSpawn(pLightEntity);
	}
}