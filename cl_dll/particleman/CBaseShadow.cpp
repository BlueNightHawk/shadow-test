#include "hud.h"
#include "cl_util.h"

#undef clamp

#include <algorithm>

#include "event_api.h"
#include "triangleapi.h"

#include "particleman.h"
#include "particleman_internal.h"
#include "CBaseParticle.h"

#include "pm_defs.h"
#include "pmtrace.h"

#include "CBaseShadow.h"

#include "PlatformHeaders.h"

#include <gl/GL.h>
#include <gl/GLU.h>

void CBaseShadow::Draw()
{
	CBaseParticle::Draw();
}