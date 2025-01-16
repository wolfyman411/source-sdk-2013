//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
// Temperature.cpp
//
// implementation of CHudTemperature class
//
#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "view.h"

#include "iclientmode.h"

#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>

#include <vgui/ILocalize.h>

using namespace vgui;

#include "hudelement.h"
#include "hud_numericdisplay.h"

#include "convar.h"

#include "c_hl2_playerlocaldata.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define INIT_TEMPERATURE 20

//-----------------------------------------------------------------------------
// Purpose: Temperature panel
//-----------------------------------------------------------------------------
class CHudTemperature : public CHudElement, public CHudNumericDisplay
{
	DECLARE_CLASS_SIMPLE( CHudTemperature, CHudNumericDisplay );

public:
	CHudTemperature( const char *pElementName );
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual void OnThink();

private:
	// old variables
	int		m_iTemperature;
	
	int		m_bitsDamage;
};	

DECLARE_HUDELEMENT( CHudTemperature );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudTemperature::CHudTemperature( const char *pElementName ) : CHudElement( pElementName ), CHudNumericDisplay(NULL, "HudTemperature")
{
	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudTemperature::Init()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudTemperature::Reset()
{
	m_iTemperature		= INIT_TEMPERATURE;
	m_bitsDamage	= 0;

	wchar_t *tempString = g_pVGuiLocalize->Find("#Valve_Hud_TEMPERATURE");

	if (tempString)
	{
		SetLabelText(tempString);
	}
	else
	{
		SetLabelText(L"TEMPERATURE");
	}
	SetDisplayValue( m_iTemperature );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudTemperature::VidInit()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudTemperature::OnThink()
{
	int newTemperature = 20;
	C_BasePlayer *local = C_BasePlayer::GetLocalPlayer();
	C_HL2PlayerLocalData* localData = dynamic_cast< C_HL2PlayerLocalData* >( local );

	if ( local )
	{
		// Never below zero
		newTemperature = MAX( localData->m_flTemperature, 0 );
	}

	// Only update the fade if we've changed temperature
	if ( newTemperature == localData->m_flTemperature )
	{
		return;
	}

	m_iTemperature = newTemperature;

	SetDisplayValue(m_iTemperature);
}