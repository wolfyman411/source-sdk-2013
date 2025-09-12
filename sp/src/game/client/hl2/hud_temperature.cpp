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

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define INIT_TEMPERATURE 70.0f

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
	float		m_flTemperature;
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
    m_flTemperature = INIT_TEMPERATURE;

	wchar_t *tempString = g_pVGuiLocalize->Find("#Valve_Hud_TEMPERATURE");

	if (tempString)
	{
		SetLabelText(tempString);
	}
	else
	{
		SetLabelText(L"TEMP");
	}

	SetDisplayValue( m_flTemperature );
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
    C_BasePlayer* local = C_BasePlayer::GetLocalPlayer();

    float newTemperature = local->m_flTemperature;
	
	// Only update the fade if we've changed temperature
	if ( newTemperature == m_flTemperature ) return;

	m_flTemperature = local->m_flTemperature;

    if ( m_flTemperature <= 0.0f )
    {
        g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "TemperatureMinimum" );
    }
    else if ( m_flTemperature >= 100.0f )
    {
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "TemperatureMaximum" );
	}
	else if ( m_flTemperature >= 100.0f ) {
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "TemperatureHalf" );
	}

	SetDisplayValue( m_flTemperature );
}