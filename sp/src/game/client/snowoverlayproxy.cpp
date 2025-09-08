// /========= Copyright Bernt Andreas Eide, All rights reserved. ============//
//
// Purpose: Handles drawing a snow overlay on the player's hands or weapon
//          when appropriate (e.g., when in snowy environments). (overlay)
//
//=============================================================================//

#include "cbase.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterialproxy.h"
#include "baseviewmodel_shared.h"
#include "c_baseplayer.h"
#include "toolframework_client.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class C_SnowyTextureProxy : public IMaterialProxy
{
public:
	C_SnowyTextureProxy();
	virtual ~C_SnowyTextureProxy();

	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	C_BaseEntity *BindArgToEntity( void *pArg );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release() { delete this; }
	IMaterial *GetMaterial();

private:
	IMaterialVar *m_pBlendFactor;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
C_SnowyTextureProxy::C_SnowyTextureProxy()
{
	m_pBlendFactor = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
C_SnowyTextureProxy::~C_SnowyTextureProxy()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_SnowyTextureProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool found;

	m_pBlendFactor = pMaterial->FindVar( "$detailblendfactor", &found, false );
	if ( !found )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_BaseEntity *C_SnowyTextureProxy::BindArgToEntity( void *pArg )
{
	IClientRenderable *pRend = (IClientRenderable *)pArg;
	return pRend ? pRend->GetIClientUnknown()->GetBaseEntity() : NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_SnowyTextureProxy::OnBind( void *pC_BaseEntity )
{
	if ( !pC_BaseEntity )
		return;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	C_BaseViewModel *pViewModel = dynamic_cast<C_BaseViewModel *>( pEntity );
	if ( pViewModel )
	{
		C_BasePlayer *pOwner = ToBasePlayer( pViewModel->GetOwner() );
		if ( pOwner )
			m_pBlendFactor->SetFloatValue( pOwner->m_bShouldDrawSnowOverlay ? 1.0f : 0.0f );
	}

	if ( ToolsEnabled() )
		ToolFramework_RecordMaterialParams( GetMaterial() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
IMaterial *C_SnowyTextureProxy::GetMaterial()
{
	return m_pBlendFactor->GetOwningMaterial();
}

EXPOSE_INTERFACE( C_SnowyTextureProxy, IMaterialProxy, "SnowTexture" IMATERIAL_PROXY_INTERFACE_VERSION );