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
// r_light.c

#include "quakedef.h"

int	r_dlightframecount;



/*
==================
R_AnimateLight

Added lightstyle interpolation. FROM MH!!!
==================
*/
void R_AnimateLight (void)
{
	int         j, k;
	float      l;
	int         flight;
	int         clight;
	float      lerpfrac;
	float      backlerp;
	extern cvar_t	r_interpolate_light;

	if (r_dynamic.value == 2)//r_dynamic 2 draws the 1st frame of the light animation then stops there; this allows certain maps to be appropriately lit, but not changing.(todo just take an average of the whole animation and use 1 single value)
		return;

	// light animations
	// 'm' is normal light, 'a' is no light, 'z' is double bright
	flight = (int) floor (cl.time * 10);
	clight = (int) ceil (cl.time * 10);
	lerpfrac = (cl.time * 10) - flight;
	backlerp = 1.0f - lerpfrac;

	for (j = 0; j < MAX_LIGHTSTYLES; j++)
	{
		if (!cl_lightstyle[j].length)
		{
		 // was 256, changed to 264 for consistency
		 d_lightstylevalue[j] = 264;
		 continue;
		}
		else if (cl_lightstyle[j].length == 1)
		{
		 // single length style so don't bother interpolating
		 d_lightstylevalue[j] = 22 * (cl_lightstyle[j].map[0] - 'a');
		 continue;
		}

		if (r_interpolate_light.value)
		{
			// interpolate animating light
			// frame just gone
			k = flight % cl_lightstyle[j].length;
			k = cl_lightstyle[j].map[k] - 'a';
			l = (float) (k * 22) * backlerp;

			// upcoming frame
			k = clight % cl_lightstyle[j].length;
			k = cl_lightstyle[j].map[k] - 'a';
			l += (float) (k * 22) * lerpfrac;

			d_lightstylevalue[j] = (int) l;
		}
		else
		{
			k = flight % cl_lightstyle[j].length;
			k = cl_lightstyle[j].map[k] - 'a';
			k = k * 22;
			d_lightstylevalue[j] = k;
		}
	}   
}

/*
=============================================================================

DYNAMIC LIGHTS BLEND RENDERING

=============================================================================
*/

float	bubble_sintable[17], bubble_costable[17];

void R_InitBubble (void)
{
	int	i;
	float	a, *bub_sin, *bub_cos;

	bub_sin = bubble_sintable;
	bub_cos = bubble_costable;

	for (i=16 ; i>=0 ; i--)
	{
		a = i/16.0 * M_PI*2;
		*bub_sin++ = sin(a);
		*bub_cos++ = cos(a);
	}
}

float bubblecolor[NUM_DLIGHTTYPES][4] = 
{
	{0.2, 0.1, 0.05},		// dimlight or brightlight (lt_default)
	{0.2, 0.1, 0.05},		// muzzleflash
	{0.2, 0.1, 0.05},		// explosion
	{0.2, 0.1, 0.05},		// rocket
	{0.5, 0.05, 0.05},		// red
	{0.05, 0.05, 0.3},		// blue
	{0.5, 0.05, 0.4},		// red + blue
	{ 0.05, 0.45, 0.05},	// green
	{ 0.5, 0.5, 0},			// yellow
	{ 0.5, 0.5, 0.5},		// white
};

void R_RenderDlight (dlight_t *light)
{
	int	i, j;
	vec3_t	v, v_right, v_up;
	float	length, rad, *bub_sin, *bub_cos;

// don't draw our own powerup glow - OUTDATED: see below, why
//	if (light->key == cl.viewentity)
//		return;

	rad = light->radius * 0.35;
	VectorSubtract (light->origin, r_origin, v);
	length = VectorNormalize (v);

	if (length < rad)
	{
		// view is inside the dlight
// joe: this looks ugly, so I decided NOT TO use it...
//		V_AddLightBlend (1, 0.5, 0, light->radius * 0.0003);
		return;
	}

	glBegin (GL_TRIANGLE_FAN);

	if (light->type == lt_explosion2 || light->type == lt_explosion3)
		glColor3fv (ExploColor);
	else
		glColor3fv (bubblecolor[light->type]);

	VectorVectors(v, v_right, v_up);

	if (length - rad > 8)
		VectorScale (v, rad, v);
	else
	// make sure the light bubble will not be clipped by near z clip plane
		VectorScale (v, length - 8, v);

	VectorSubtract (light->origin, v, v);

	glVertex3fv (v);
	glColor3f (0, 0, 0);

	bub_sin = bubble_sintable;
	bub_cos = bubble_costable;

	for (i=16; i>=0; i--)
	{
		for (j=0 ; j<3 ; j++)
			v[j] = light->origin[j] + (v_right[j]*(*bub_cos) + v_up[j]*(*bub_sin)) * rad;
		bub_sin++; 
		bub_cos++;
		glVertex3fv (v);
	}

	glEnd ();
}

/*
=============
R_RenderDlights
=============
*/
void R_RenderDlights (void)
{
	int		i;
	dlight_t	*l;

	if (!gl_flashblend.value)
		return;

	glDepthMask (GL_FALSE);
	glDisable (GL_TEXTURE_2D);
	
	glShadeModel (GL_SMOOTH);
	glEnable (GL_BLEND);
	glBlendFunc (GL_ONE, GL_ONE);

	l = cl_dlights;
	
	for (i = 0 ; i < MAX_DLIGHTS ; i++, l++)
	{
		if (l->die < cl.time || !l->radius)
			continue;
		R_RenderDlight (l);
	}	
	
	glColor3f (1, 1, 1);
	glDepthMask (GL_TRUE);
	glEnable (GL_TEXTURE_2D);
	glShadeModel (GL_FLAT);
	glDisable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

void R_MarkLights (dlight_t *light, int num, mnode_t *node)
{
   mplane_t   *splitplane;
   float      dist;
   msurface_t   *surf;
   int         i;

   if (node->contents < 0)
      return;

   splitplane = node->plane;
   dist = DotProduct (light->origin, splitplane->normal) - splitplane->dist;

   if (dist > light->radius)
   {
      R_MarkLights (light, num, node->children[0]);
      return;
   }

   if (dist < -light->radius)
   {
      R_MarkLights (light, num, node->children[1]);
      return;
   }

   // mark the polygons
   surf = cl.worldmodel->surfaces + node->firstsurface;

   for (i = 0; i < node->numsurfaces; i++, surf++)
   {
      if (surf->dlightframe != r_framecount)
      {
         memset (surf->dlightbits, 0, sizeof (surf->dlightbits));
         surf->dlightframe = r_framecount;
      }

      surf->dlightbits[num >> 5] |= 1 << (num & 31);
   }

   R_MarkLights (light, num, node->children[0]);
   R_MarkLights (light, num, node->children[1]);
}

void R_PushDlights (mnode_t *headnode)
{
   int i;
   dlight_t *l = cl_dlights;

   for (i = 0; i < MAX_DLIGHTS; i++, l++)
   {
      if (l->die < cl.time || (l->radius <= 0))
         continue;

      R_MarkLights (l, i, headnode);
   }
}
/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

mplane_t	*lightplane;
vec3_t		lightspot, lightcolor;

int RecursiveLightPoint (vec3_t color, mnode_t *node, vec3_t start, vec3_t end)
{
	float	front, back, frac;
	vec3_t	mid;

loc0:
	if (node->contents < 0)
		return false;		// didn't hit anything
	
// calculate mid point
	if (node->plane->type < 3)
	{
		front = start[node->plane->type] - node->plane->dist;
		back = end[node->plane->type] - node->plane->dist;
	}
	else
	{
		front = DotProduct(start, node->plane->normal) - node->plane->dist;
		back = DotProduct(end, node->plane->normal) - node->plane->dist;
	}

	// optimized recursion
	if ((back < 0) == (front < 0))
	{
		node = node->children[front < 0];
		goto loc0;
	}
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;
	
// go down front side
	if (RecursiveLightPoint(color, node->children[front < 0], start, mid))
	{
		return true;	// hit something
	}
	else
	{
		int		i, ds, dt;
		msurface_t	*surf;

	// check for impact on this node
		VectorCopy (mid, lightspot);
		lightplane = node->plane;

		surf = cl.worldmodel->surfaces + node->firstsurface;
		for (i = 0 ; i < node->numsurfaces ; i++, surf++)
		{
			if (surf->flags & SURF_DRAWTILED)
				continue;	// no lightmaps

			ds = (int)((float)DotProduct (mid, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3]);
			dt = (int)((float)DotProduct (mid, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3]);

			if (ds < surf->texturemins[0] || dt < surf->texturemins[1])
				continue;
			
			ds -= surf->texturemins[0];
			dt -= surf->texturemins[1];
			
			if (ds > surf->extents[0] || dt > surf->extents[1])
				continue;

			if (surf->samples)
			{
				// enhanced to interpolate lighting
				byte	*lightmap;
				int	maps, line3, dsfrac = ds & 15, dtfrac = dt & 15, r00 = 0, g00 = 0, b00 = 0, r01 = 0, g01 = 0, b01 = 0, r10 = 0, g10 = 0, b10 = 0, r11 = 0, g11 = 0, b11 = 0;
				float	scale;

				line3 = ((surf->extents[0] >> 4) + 1) * 3;
				lightmap = surf->samples + ((dt>>4) * ((surf->extents[0]>>4)+1) + (ds>>4))*3; // LordHavoc: *3 for color

				for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ; maps++)
				{
					scale = (float)d_lightstylevalue[surf->styles[maps]] * 1.0 / 256.0;
					r00 += (float)lightmap[0] * scale;
					g00 += (float)lightmap[1] * scale;
					b00 += (float)lightmap[2] * scale;

					r01 += (float)lightmap[3] * scale;
					g01 += (float)lightmap[4] * scale;
					b01 += (float)lightmap[5] * scale;

					r10 += (float)lightmap[line3+0] * scale;
					g10 += (float)lightmap[line3+1] * scale;
					b10 += (float)lightmap[line3+2] * scale;

					r11 += (float)lightmap[line3+3] * scale;
					g11 += (float)lightmap[line3+4] * scale;
					b11 += (float)lightmap[line3+5] * scale;

					lightmap += ((surf->extents[0] >> 4) + 1) * ((surf->extents[1] >> 4) +1 ) * 3; // LordHavoc: *3 for colored lighting
				}
				color[0] += (float)((int)((((((((r11 - r10) * dsfrac) >> 4) + r10) 
					- ((((r01 - r00) * dsfrac) >> 4) + r00)) * dtfrac) >> 4) 
					+ ((((r01 - r00) * dsfrac) >> 4) + r00)));
				color[1] += (float)((int)((((((((g11 - g10) * dsfrac) >> 4) + g10) 
					- ((((g01 - g00) * dsfrac) >> 4) + g00)) * dtfrac) >> 4) 
					+ ((((g01 - g00) * dsfrac) >> 4) + g00)));
				color[2] += (float)((int)((((((((b11 - b10) * dsfrac) >> 4) + b10) 
					- ((((b01 - b00) * dsfrac) >> 4) + b00)) * dtfrac) >> 4) 
					+ ((((b01 - b00) * dsfrac) >> 4) + b00)));
			}

			return true;	// success
		}

	// go down back side
		return RecursiveLightPoint (color, node->children[front >= 0], mid, end);

	}
}
int R_LightPoint (vec3_t p)
{
	vec3_t	end;
	
	if ((developer.value && r_fullbright.value) || (!cl.worldmodel->lightdata))
	{
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 255;
		return 255;
	}
	
	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;
	
	VectorClear (lightcolor);
	RecursiveLightPoint (lightcolor, cl.worldmodel->nodes, p, end);

	return (lightcolor[0] + lightcolor[1] + lightcolor[2]) / 3.0;
}
/*
=============================================================================

VERTEX LIGHTING

=============================================================================
*/

float	vlight_pitch = 45;
float	vlight_yaw = 45;
float	vlight_highcut = 128;
float	vlight_lowcut = 60;

extern	float	r_avertexnormals[NUMVERTEXNORMALS][3];

byte	anorm_pitch[NUMVERTEXNORMALS];
byte	anorm_yaw[NUMVERTEXNORMALS];

byte	vlighttable[256][256];

float R_GetVertexLightValue (byte ppitch, byte pyaw, float apitch, float ayaw)
{
	int	pitchofs, yawofs;
	float	retval;

	pitchofs = ppitch + (apitch * 256 / 360);
	yawofs = pyaw + (ayaw * 256 / 360);
	while (pitchofs > 255)
		pitchofs -= 256;
	while (yawofs > 255)
		yawofs -= 256;
	while (pitchofs < 0)
		pitchofs += 256;
	while (yawofs < 0)
		yawofs += 256;

	retval = vlighttable[pitchofs][yawofs];

	return retval / 256;
}

float R_LerpVertexLight (byte ppitch1, byte pyaw1, byte ppitch2, byte pyaw2, float ilerp, float apitch, float ayaw)
{
	float	lightval1, lightval2, val;

	lightval1 = R_GetVertexLightValue (ppitch1, pyaw1, apitch, ayaw);
	lightval2 = R_GetVertexLightValue (ppitch2, pyaw2, apitch, ayaw);

	val = (lightval2 * ilerp) + (lightval1 * (1 - ilerp));

	return val;
}

void R_ResetAnormTable (void)
{
	int	i, j;
	float	forward, yaw, pitch, angle, sp, sy, cp, cy, precut;
	vec3_t	normal, lightvec;

	// Define the light vector here
	angle = DEG2RAD(vlight_pitch);
	sy = sin(angle);
	cy = cos(angle);
	angle = DEG2RAD(-vlight_yaw);
	sp = sin(angle);
	cp = cos(angle);
	lightvec[0] = cp*cy;
	lightvec[1] = cp*sy;
	lightvec[2] = -sp;

	// First thing that needs to be done is the conversion of the
	// anorm table into a pitch/yaw table

	for (i=0 ; i<NUMVERTEXNORMALS ; i++)
	{
		if (r_avertexnormals[i][1] == 0 && r_avertexnormals[i][0] == 0)
		{
			yaw = 0;
			if (r_avertexnormals[i][2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{
			yaw = (int)(atan2(r_avertexnormals[i][1], r_avertexnormals[i][0]) * 57.295779513082320);
			if (yaw < 0)
				yaw += 360;
	
			forward = sqrt(r_avertexnormals[i][0]*r_avertexnormals[i][0] + r_avertexnormals[i][1]*r_avertexnormals[i][1]);
			pitch = (int)(atan2(r_avertexnormals[i][2], forward) * 57.295779513082320);
			if (pitch < 0)
				pitch += 360;
		}
		anorm_pitch[i] = pitch * 256 / 360;
		anorm_yaw[i] = yaw * 256 / 360;
	}

	// Next, a light value table must be constructed for pitch/yaw offsets
	// DotProduct values

	// DotProduct values never go higher than 2, so store bytes as
	// (product * 127.5)

	for (i=0 ; i<256 ; i++)
	{
		angle = DEG2RAD(i * 360 / 256);
		sy = sin(angle);
		cy = cos(angle);
		for (j=0 ; j<256 ; j++)
		{
			angle = DEG2RAD(j * 360 / 256);
			sp = sin(angle);
			cp = cos(angle);

			normal[0] = cp*cy;
			normal[1] = cp*sy;
			normal[2] = -sp;

			precut = ((DotProduct(normal, lightvec) + 2) * 31.5);
			precut = (precut - (vlight_lowcut)) * 256 / (vlight_highcut - vlight_lowcut);
			if (precut > 255)
				precut = 255;
			if (precut < 0)
				precut = 0;
			vlighttable[i][j] = precut;
		}
	}
}

void R_InitVertexLights (void)
{
	R_ResetAnormTable ();
}
