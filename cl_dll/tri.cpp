//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any
#include <algorithm>
#include <cmath>

#include "hud.h"
#include "cl_util.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "Exports.h"

#include "particleman.h"
#include "tri.h"

#include "studio.h"
#include "com_model.h"
#include "r_studioint.h"

#include "pm_defs.h"
#include "pmtrace.h"
#include "event_api.h"

extern engine_studio_api_s IEngineStudio;

extern IParticleMan* g_pParticleMan;

void TRI_DrawSpotShadows();

cl_entity_t* g_pShadowQueue[512];

/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
=================
*/
void DLLEXPORT HUD_DrawNormalTriangles()
{
	//	RecClDrawNormalTriangles();

	gHUD.m_Spectator.DrawOverview();
}

/*
=================
HUD_DrawTransparentTriangles

Render any triangles with transparent rendermode needs here
=================
*/
void DLLEXPORT HUD_DrawTransparentTriangles()
{
	//	RecClDrawTransparentTriangles();
	TRI_DrawSpotShadows();

	if (g_pParticleMan)
		g_pParticleMan->Update();
}


void cl_sprite_s::Draw()
{
	Vector resultColor;
	Vector vColor;
	float intensity = 0.0;

	if ((m_iRenderFlags & LIGHT_NONE) == 0)
	{
		gEngfuncs.pTriAPI->LightAtPoint(m_vOrigin, vColor);

		intensity = (vColor.x + vColor.y + vColor.z) / 3.0;
	}

	if ((m_iRenderFlags & LIGHT_NONE) != 0)
	{
		resultColor = m_vColor;
	}
	else if ((m_iRenderFlags & LIGHT_COLOR) != 0)
	{
		resultColor.x = vColor.x / (m_vColor.x * 255);
		resultColor.y = vColor.y / (m_vColor.y * 255);
		resultColor.z = vColor.z / (m_vColor.z * 255);
	}
	else if ((m_iRenderFlags & LIGHT_INTENSITY) != 0)
	{
		resultColor.x = intensity / (m_vColor.x * 255);
		resultColor.y = intensity / (m_vColor.y * 255);
		resultColor.z = intensity / (m_vColor.z * 255);
	}

	resultColor.x = std::clamp(resultColor.x, 0.f, 255.f);
	resultColor.y = std::clamp(resultColor.y, 0.f, 255.f);
	resultColor.z = std::clamp(resultColor.z, 0.f, 255.f);

	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors(m_vAngles, forward, right, up);

	const float radius = m_flSize;
	const Vector width = right * radius * m_flStretchX;
	const Vector height = up * radius * m_flStretchY;

	// TODO: shouldn't this be accounting for stretch Y?
	const Vector lowLeft = m_vOrigin - (width * 0.5) - (up * radius * 0.5);

	const Vector lowRight = lowLeft + width;
	const Vector topLeft = lowLeft + height;
	const Vector topRight = lowRight + height;

	gEngfuncs.pTriAPI->SpriteTexture(m_pTexture, m_iFrame);
	gEngfuncs.pTriAPI->RenderMode(m_iRendermode);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);

	gEngfuncs.pTriAPI->Begin(TRI_QUADS);
	gEngfuncs.pTriAPI->Color4f(resultColor.x / 255, resultColor.y / 255, resultColor.z / 255, m_flBrightness / 255);

	gEngfuncs.pTriAPI->TexCoord2f(0, 0);
	gEngfuncs.pTriAPI->Vertex3fv(topLeft);

	gEngfuncs.pTriAPI->TexCoord2f(0, 1);
	gEngfuncs.pTriAPI->Vertex3fv(lowLeft);

	gEngfuncs.pTriAPI->TexCoord2f(1, 1);
	gEngfuncs.pTriAPI->Vertex3fv(lowRight);

	gEngfuncs.pTriAPI->TexCoord2f(1, 0);
	gEngfuncs.pTriAPI->Vertex3fv(topRight);

	gEngfuncs.pTriAPI->End();

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
	gEngfuncs.pTriAPI->CullFace(TRI_FRONT);
}

void cl_sprite_s::InitSprite(model_s *spr)
{
	m_flSize = 10;

	m_flStretchX = m_flStretchY = 1;

	m_pTexture = nullptr;
	m_flBrightness = 255;

	m_iFramerate = 0;
	m_iNumFrames = 0;
	m_iFrame = 0;

	m_iRendermode = kRenderNormal;
	m_iRenderFlags = 0;

	m_vColor.x = 255;
	m_vColor.y = 255;
	m_vColor.z = 255;

	m_pTexture = spr;

	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors(m_vAngles, forward, right, up);

	const Vector scaledRight = right * m_flSize;
	const Vector scaledUp = up * m_flSize;

	m_vLowLeft = m_vOrigin - scaledRight * 0.5 - scaledUp * 0.5;

	// TODO: not sure if these are correct. If low left is half of the scaled directions then the full direction * 2 results in double size particles.
	m_vLowRight = m_vLowLeft + scaledRight + scaledRight;
	m_vTopLeft = m_vLowLeft + scaledUp + scaledUp;
}

void InitShadowSpriteForEnt(cl_entity_s *e, cl_sprite_t* s)
{
	const model_s* pSprite = IEngineStudio.Mod_ForName("sprites/shadows.spr", 0);
	Vector angles;

	s->InitSprite((model_s*)pSprite);

	// Use the player's info instead of viewmodel info for better results
	if (e == gEngfuncs.GetViewModel())
	{
		e = gEngfuncs.GetLocalPlayer();
	}

	int idx = e->index;
	int shadowidx = idx - 1;

	Vector vecSrc = e->origin;
	Vector vecEnd = e->origin - Vector(0, 0, 8192);
	pmtrace_s tr;

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(idx - 1);

	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	// Trace to ground
	gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_WORLD_ONLY | PM_GLASS_IGNORE, idx, &tr);

	gEngfuncs.pEventAPI->EV_PopPMStates();

	VectorAngles(tr.plane.normal, (float*)&angles);
	angles[0] *= -1;

	s->m_vOrigin = tr.endpos + Vector(0, 0, 1);
	s->m_vAngles = angles;

	s->m_flSize = 60;
	s->m_flBrightness = 50;
	s->m_iRendermode = kRenderTransAlpha;
	s->m_vColor = Vector(0, 0, 0);

	s->SetLightFlag(LIGHT_NONE);
}

void SetEntQueueForShadow(cl_entity_s *ent)
{
	for (int i = 0; i < 512; i++)
	{
		if (g_pShadowQueue[i] == ent)
			break;

		if (g_pShadowQueue[i])
			continue;

		if (!g_pShadowQueue[i])
		{
			g_pShadowQueue[i] = ent;
			break;
		}
	}
}

void TRI_DrawSpotShadows()
{
	for (int i = 0; i < 512; i++)
	{
		cl_entity_s* e = g_pShadowQueue[i];

		if (!e)
			continue;
		if (!e->model || e->model->type != mod_studio)
			continue;
		if (e->curstate.effects & EF_NODRAW)
			continue;

		cl_sprite_t s;
		InitShadowSpriteForEnt(e, &s);
		s.Draw();

		e = g_pShadowQueue[i] = nullptr;
	}
}