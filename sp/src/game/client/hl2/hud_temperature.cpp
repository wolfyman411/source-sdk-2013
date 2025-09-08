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
#include "c_basehlplayer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define INIT_TEMPERATURE 33.0f

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
	float		m_iTemperature;
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

	wchar_t *tempString = g_pVGuiLocalize->Find("#Valve_Hud_TEMPERATURE");

	if (tempString)
	{
		SetLabelText(tempString);
	}
	else
	{
		SetLabelText(L"TEMP");
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
	C_BaseHLPlayer* local = dynamic_cast< C_BaseHLPlayer* >( C_BasePlayer::GetLocalPlayer() );
	if ( !local )
		return;

	float newTemperature = local->m_HL2Local.m_flTemperature;
	
	if ( local->m_HL2Local.m_flTemperature >= local->m_HL2Local.m_flMaxTemperature ) {
		newTemperature = local->m_HL2Local.m_flMaxTemperature;
	} else if ( local->m_HL2Local.m_flTemperature <= local->m_HL2Local.m_flMinTemperature ) {
		newTemperature = local->m_HL2Local.m_flMinTemperature;
	}

	// Only update the fade if we've changed temperature
	if ( newTemperature == m_iTemperature ) return;

	m_iTemperature = local->m_HL2Local.m_flTemperature;

	if ( m_iTemperature <= local->m_HL2Local.m_flMinTemperature ) {
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "TemperatureMinimum" );
	}
	else if ( m_iTemperature >= local->m_HL2Local.m_flMaxTemperature ) {
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "TemperatureMaximum" );
	}
	else if ( m_iTemperature >= local->m_HL2Local.m_flMaxTemperature / 2 ) {
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "TemperatureHalf" );
	}

	SetDisplayValue(m_iTemperature);
}