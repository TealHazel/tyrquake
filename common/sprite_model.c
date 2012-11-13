/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <string.h>

#include "common.h"
#include "d_iface.h"
#include "model.h"
#include "sys.h"
#include "zone.h"

/*
=================
Mod_LoadSpriteFrame
=================
*/
static void *
Mod_LoadSpriteFrame(void *pin, mspriteframe_t **ppframe, const char *loadname,
		    int framenum)
{
    dspriteframe_t *pinframe;
    mspriteframe_t *pspriteframe;
    int i, width, height, size, origin[2];
    unsigned short *ppixout;
    byte *ppixin;

    pinframe = (dspriteframe_t *)pin;

    width = LittleLong(pinframe->width);
    height = LittleLong(pinframe->height);
    size = width * height;

    pspriteframe =
	Hunk_AllocName(sizeof(mspriteframe_t) + size * r_pixbytes, loadname);

    memset(pspriteframe, 0, sizeof(mspriteframe_t) + size);
    *ppframe = pspriteframe;

    pspriteframe->width = width;
    pspriteframe->height = height;
    origin[0] = LittleLong(pinframe->origin[0]);
    origin[1] = LittleLong(pinframe->origin[1]);

    pspriteframe->up = origin[1];
    pspriteframe->down = origin[1] - height;
    pspriteframe->left = origin[0];
    pspriteframe->right = width + origin[0];

    if (r_pixbytes == 1) {
	memcpy(&pspriteframe->pixels[0], (byte *)(pinframe + 1), size);
    } else if (r_pixbytes == 2) {
	ppixin = (byte *)(pinframe + 1);
	ppixout = (unsigned short *)&pspriteframe->pixels[0];

	for (i = 0; i < size; i++)
	    ppixout[i] = d_8to16table[ppixin[i]];
    } else {
	Sys_Error("%s: driver set invalid r_pixbytes: %d", __func__,
		  r_pixbytes);
    }

    return (void *)((byte *)pinframe + sizeof(dspriteframe_t) + size);
}


/*
=================
Mod_LoadSpriteGroup
=================
*/
static void *
Mod_LoadSpriteGroup(void *pin, mspriteframe_t **ppframe, const char *loadname,
		    int framenum)
{
    dspritegroup_t *pingroup;
    mspritegroup_t *pspritegroup;
    int i, numframes;
    dspriteinterval_t *pin_intervals;
    float *poutintervals;
    void *ptemp;

    pingroup = (dspritegroup_t *)pin;

    numframes = LittleLong(pingroup->numframes);

    pspritegroup = Hunk_AllocName(sizeof(mspritegroup_t) +
				  (numframes -
				   1) * sizeof(pspritegroup->frames[0]),
				  loadname);

    pspritegroup->numframes = numframes;

    *ppframe = (mspriteframe_t *)pspritegroup;

    pin_intervals = (dspriteinterval_t *)(pingroup + 1);

    poutintervals = Hunk_AllocName(numframes * sizeof(float), loadname);

    pspritegroup->intervals = poutintervals;

    for (i = 0; i < numframes; i++) {
	*poutintervals = LittleFloat(pin_intervals->interval);
	if (*poutintervals <= 0.0)
	    Sys_Error("%s: interval <= 0", __func__);

	poutintervals++;
	pin_intervals++;
    }

    ptemp = (void *)pin_intervals;

    for (i = 0; i < numframes; i++) {
	ptemp = Mod_LoadSpriteFrame(ptemp, &pspritegroup->frames[i], loadname,
				    framenum * 100 + i);
    }

    return ptemp;
}


/*
=================
Mod_LoadSpriteModel
=================
*/
void
Mod_LoadSpriteModel(model_t *mod, void *buffer, const char *loadname)
{
    int i;
    int version;
    dsprite_t *pin;
    msprite_t *psprite;
    int numframes;
    int size;
    dspriteframetype_t *pframetype;

    pin = (dsprite_t *) buffer;

    version = LittleLong(pin->version);
    if (version != SPRITE_VERSION)
	Sys_Error("%s: %s has wrong version number (%i should be %i)",
		  __func__, mod->name, version, SPRITE_VERSION);

    numframes = LittleLong(pin->numframes);
    size = sizeof(msprite_t) + (numframes - 1) * sizeof(psprite->frames);
    psprite = Hunk_AllocName(size, loadname);
    mod->cache.data = psprite;

    psprite->type = LittleLong(pin->type);
    psprite->maxwidth = LittleLong(pin->width);
    psprite->maxheight = LittleLong(pin->height);
    psprite->beamlength = LittleFloat(pin->beamlength);
    mod->synctype = LittleLong(pin->synctype);
    psprite->numframes = numframes;

    mod->mins[0] = mod->mins[1] = -psprite->maxwidth / 2;
    mod->maxs[0] = mod->maxs[1] = psprite->maxwidth / 2;
    mod->mins[2] = -psprite->maxheight / 2;
    mod->maxs[2] = psprite->maxheight / 2;

//
// load the frames
//
    if (numframes < 1)
	Sys_Error("%s: Invalid # of frames: %d", __func__, numframes);

    mod->numframes = numframes;
    mod->flags = 0;

    pframetype = (dspriteframetype_t *) (pin + 1);

    for (i = 0; i < numframes; i++) {
	spriteframetype_t frametype;

	frametype = LittleLong(pframetype->type);
	psprite->frames[i].type = frametype;

	if (frametype == SPR_SINGLE) {
	    pframetype = (dspriteframetype_t *)
		Mod_LoadSpriteFrame(pframetype + 1,
				    &psprite->frames[i].frameptr, loadname, i);
	} else {
	    pframetype = (dspriteframetype_t *)
		Mod_LoadSpriteGroup(pframetype + 1,
				    &psprite->frames[i].frameptr, loadname, i);
	}
    }

    mod->type = mod_sprite;
}
