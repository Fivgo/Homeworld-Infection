/*=============================================================================
    Name    : NavLights.c
    Purpose : Control operation of NAV lights on ships.

    Created 6/21/1997 by agarden
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "NavLights.h"

#include "glinc.h"
#include "Matrix.h"
#include "Memory.h"
#include "prim3d.h"
#include "render.h"
#include "Universe.h"
#include "LOD.h"            // HWI-FEATURE(full-detail): cat_lodDrawLevel

#ifndef SW_Render
    #ifdef _WIN32
        #include <windows.h>
    #endif
#endif

/*-----------------------------------------------------------------------------
    Name        : navLightBillboardEnable
    Description : setup modelview matrix for rendering billboarded sprites
    Inputs      : s - ship to obtain coordinate system from
                  nls - NAV Light static info
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void navLightBillboardEnable(Ship *s, NAVLightStatic *nls)
{
    vector src, dst;

    //object -> worldspace
    src = nls->position;
    matMultiplyMatByVec(&dst, &s->rotatable.rotinfo.coordsys, &src);
    vecAddTo(dst, s->obj.posinfo.position);

    //setup billboarding
    glPushMatrix();
    rndBillboardEnable(&dst);
    glDisable(GL_CULL_FACE);
}

/*-----------------------------------------------------------------------------
    Name        : navLightBillboardDisable
    Description : reset modelview matrix to non-billboard state
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void navLightBillboardDisable(void)
{
    //undo billboarding
    rndBillboardDisable();
    glPopMatrix();
    glEnable(GL_CULL_FACE);
}

/*-----------------------------------------------------------------------------
    Name        : navLightStaticInfoDelete
    Description : Delete the static info of a set of nav lights
    Inputs      : staticInfo - array of navlight structures to free
    Outputs     : unregisters the navlight textures, if any.
    Return      :
----------------------------------------------------------------------------*/
void navLightStaticInfoDelete(NAVLightStaticInfo *staticInfo)
{
    sdword i, num = staticInfo->numNAVLights;
    NAVLightStatic *navLightStatic = staticInfo->navlightstatics;

    dbgAssertOrIgnore(staticInfo != NULL);

    for( i=0 ; i < num ; i++, navLightStatic++)
    {
        if (navLightStatic->texturehandle != TR_InvalidHandle)
        {
            trTextureUnregister(navLightStatic->texturehandle);
        }
    }
    memFree(staticInfo);
}

/*-----------------------------------------------------------------------------
    Name        : RenderNAVLights
    Description : TODO: render sorted by projected depth value so alpha sorts correctly
    Inputs      : ship - the ship whose navlights we are to render
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void RenderNAVLights(Ship *ship)
{
   //TODO(type safety, 2026-04-20): Make a "parent" `NavLights` struct to Ship & Derelict
   // and take that as argument to avoid casting Derelicts as Ships
   sdword i;
   NAVLight *navLight;
   NAVLightInfo *navLightInfo;
   ShipStaticInfo *shipStaticInfo;
   NAVLightStatic *navLightStatic;
   vector origin = {0.0f, 0.0f, 0.0f};
   NAVLightStaticInfo *navLightStaticInfo;
   real32 fade, lightFade;
   bool32 lightOn;
   extern bool32 bFade;
   extern real32 meshFadeAlpha;

   fade = bFade ? meshFadeAlpha : 1.0f;

   shipStaticInfo = (ShipStaticInfo *)ship->staticinfo;

    navLightInfo = ship->shipOrDerelict.navLightInfo;
   if(shipStaticInfo->navlightStaticInfo && navLightInfo != NULL)
   {
      glDepthMask(GL_FALSE);
      rndAdditiveBlends(TRUE);
      lightOn = rndLightingEnable(FALSE);

      navLightStaticInfo = shipStaticInfo->navlightStaticInfo;
      navLightStatic = navLightStaticInfo->navlightstatics;
      navLight = navLightInfo->navLights;

      for( i=0 ; i<navLightStaticInfo->numNAVLights ; i++, navLight ++, navLightStatic ++)
      {
			// Account for the startdelay.
			if(navLight->lastTimeFlashed == navLightStatic->startdelay)
			{
				navLight->lastTimeFlashed = universe.totaltimeelapsed + navLightStatic->startdelay;
			}
			
			if(universe.totaltimeelapsed > navLight->lastTimeFlashed)
			{
				if(navLight->lightstate == 1)
				{
					navLight->lastTimeFlashed = universe.totaltimeelapsed + navLightStatic->flashrateoff;
				}
				else
				{
					navLight->lastTimeFlashed = universe.totaltimeelapsed + navLightStatic->flashrateon;
				}
				
				navLight->lightstate = 1 - navLight->lightstate;
			}

			// CAT (M2-R5 render): RenderNAVLights 0x004c78e0 at 0x004c79c6: a NAVLIGHT_Default light is drawn while
			//      it is lit; a NAVLIGHT_FadeOnOff light is always drawn and pulses: its fade times the part of the current on/off
			//      interval still to run, from 1 down while off and from 0 up while on; other types are not drawn.
			//      Past its LOD nothing is drawn (HW1's point sprite went with primPointSize3Fade)
			lightFade = fade;
			if (navLightStatic->lightType == NAVLIGHT_FadeOnOff)
			{
				lightFade = (navLight->lastTimeFlashed - universe.totaltimeelapsed) /
				            (navLight->lightstate ? navLightStatic->flashrateon : navLightStatic->flashrateoff) * fade;
				if (navLight->lightstate)
				{
					lightFade = 1.0f - lightFade;
				}
			}
			if (((navLightStatic->lightType == NAVLIGHT_Default) && navLight->lightstate) || (navLightStatic->lightType == NAVLIGHT_FadeOnOff))
			{
				if ((sdword)cat_lodDrawLevel(ship) <= (sdword)navLightStatic->minLOD)	// HWI-FEATURE(full-detail): the level drawn (the exe: currentLOD)
				{
					navLightBillboardEnable(ship, navLightStatic);

					if(navLightStatic->texturehandle == TR_InvalidHandle)
					{
						primCircleSolid3Fade(&origin, navLightStatic->size, 10, navLightStatic->color, lightFade);
					}
					else
					{
						primSolidTexture3Fade(&origin, navLightStatic->size, navLightStatic->color, navLightStatic->texturehandle, lightFade);
					}

					navLightBillboardDisable();
				}
			}
      }

      rndLightingEnable(lightOn);
      rndAdditiveBlends(FALSE);
      glDepthMask(GL_TRUE);
    }
}

