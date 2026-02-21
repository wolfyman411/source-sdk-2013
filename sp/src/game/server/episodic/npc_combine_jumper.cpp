#include "cbase.h"
#include "npc_combine_jumper.h"
#include "npc_combine_armored.h"
#include "props.h"

#include "tier0/memdbgon.h"

void CNPC_Combine_Jumper::Spawn( void )
{
    Precache();
    SetModel( STRING( GetModelName() ) );
    BaseClass::Spawn();
}

void CNPC_Combine_Jumper::Precache( void )
{
    if ( !GetModelName() )
    {
        SetModelName( MAKE_STRING( "models/combine_soldier.mdl" ) );
    }

    PrecacheModel( STRING( GetModelName() ) );

    BaseClass::Precache();
}