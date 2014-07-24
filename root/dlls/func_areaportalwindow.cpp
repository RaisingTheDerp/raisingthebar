//========= Copyright � 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "func_areaportalwindow.h"
#include "entitylist.h"


// The server will still send entities through a window even after it opaque 
// to allow for net lag.
#define FADE_DIST_BUFFER	10


LINK_ENTITY_TO_CLASS( func_areaportalwindow, CFuncAreaPortalWindow );


IMPLEMENT_SERVERCLASS_ST( CFuncAreaPortalWindow, DT_FuncAreaPortalWindow )
	SendPropFloat( SENDINFO(m_flFadeDist), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flFadeStartDist), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flTranslucencyLimit), 0, SPROP_NOSCALE ),

	SendPropModelIndex(SENDINFO(m_iBackgroundModelIndex) ),
END_SEND_TABLE()


BEGIN_DATADESC( CFuncAreaPortalWindow )

	DEFINE_KEYFIELD( CFuncAreaPortalWindow, m_portalNumber, FIELD_INTEGER,	"portalnumber" ),
	DEFINE_KEYFIELD( CFuncAreaPortalWindow, m_flFadeStartDist,	FIELD_FLOAT,	"FadeStartDist" ),
	DEFINE_KEYFIELD( CFuncAreaPortalWindow, m_flFadeDist,	FIELD_FLOAT,	"FadeDist" ),
	DEFINE_KEYFIELD( CFuncAreaPortalWindow, m_flTranslucencyLimit,	FIELD_FLOAT,	"TranslucencyLimit" ),
	DEFINE_KEYFIELD( CFuncAreaPortalWindow,	m_iBackgroundBModelName,FIELD_STRING,	"BackgroundBModel" ),
//	DEFINE_KEYFIELD( CFuncAreaPortalWindow,	m_iBackgroundModelIndex,FIELD_INTEGER ),

END_DATADESC()




CFuncAreaPortalWindow::CFuncAreaPortalWindow()
{
	m_iBackgroundModelIndex = -1;
}


CFuncAreaPortalWindow::~CFuncAreaPortalWindow()
{
}


void CFuncAreaPortalWindow::Spawn()
{
	Precache();

	engine->SetAreaPortalState( m_portalNumber, 1 );
}


void CFuncAreaPortalWindow::Activate()
{
	BaseClass::Activate();
	
	// Find our background model.
	CBaseEntity *pBackground = gEntList.FindEntityByName( NULL, m_iBackgroundBModelName, NULL );
	if( pBackground )
	{
		m_iBackgroundModelIndex  = modelinfo->GetModelIndex( STRING( pBackground->GetModelName() ) );
		pBackground->m_fEffects |= EF_NODRAW; // we will draw for it.
	}

	// Find our target and steal its bmodel.
	CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, m_target, NULL );
	if( pTarget )
	{
		SetModel( STRING(pTarget->GetModelName()) );
		SetAbsOrigin( pTarget->GetAbsOrigin() );
		pTarget->m_fEffects |= EF_NODRAW; // we will draw for it.
	}

	// TODO: make sure the dims get set here and it's relinked correctly.
	Relink();
}



bool CFuncAreaPortalWindow::UpdateVisibility( const Vector &vOrigin )
{
	if( !BaseClass::UpdateVisibility( vOrigin ) )
		return false;

	float flDist = CalcDistanceToAABB( GetAbsMins(), GetAbsMaxs(), vOrigin );
	if( flDist > (m_flFadeDist + FADE_DIST_BUFFER) )
	{
		engine->SetAreaPortalState( m_portalNumber, false );
		return false;
	}
	else
	{
		engine->SetAreaPortalState( m_portalNumber, true );
		return true;
	}
}

