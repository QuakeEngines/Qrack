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
// gl_mesh.c: triangle model functions

#include "quakedef.h"

/*
=================================================================

ALIAS MODEL DISPLAY LIST GENERATION

=================================================================
*/

model_t		*aliasmodel;
aliashdr_t	*paliashdr;

qboolean	used[8192];

// the command list holds counts and s/t values that are valid for
// every frame
int		commands[8192];
int		numcommands;

// all frames will have their vertexes rearranged and expanded
// so they are in the order expected by the command list
int		vertexorder[8192];
int		numorder;

int		allverts, alltris;

int		stripverts[128];
int		striptris[128];
int		stripcount;

/*
================
StripLength
================
*/
int StripLength (int starttri, int startv)
{
	int		m1, m2, j, k;
	mtriangle_t	*last, *check;

	used[starttri] = 2;

	last = &triangles[starttri];

	stripverts[0] = last->vertindex[(startv)%3];
	stripverts[1] = last->vertindex[(startv+1)%3];
	stripverts[2] = last->vertindex[(startv+2)%3];

	striptris[0] = starttri;
	stripcount = 1;

	m1 = last->vertindex[(startv+2)%3];
	m2 = last->vertindex[(startv+1)%3];

	// look for a matching triangle
nexttri:
	for (j=starttri+1, check=&triangles[starttri+1] ; j<pheader->numtris ; j++, check++)
	{
		if (check->facesfront != last->facesfront)
			continue;
		for (k=0 ; k<3 ; k++)
		{
			if (check->vertindex[k] != m1)
				continue;
			if (check->vertindex[ (k+1)%3 ] != m2)
				continue;

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if (used[j])
				goto done;

			// the new edge
			if (stripcount & 1)
				m2 = check->vertindex[ (k+2)%3 ];
			else
				m1 = check->vertindex[ (k+2)%3 ];

			stripverts[stripcount+2] = check->vertindex[ (k+2)%3 ];
			striptris[stripcount] = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}

done:
	// clear the temp used flags
	for (j=starttri+1 ; j<pheader->numtris ; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}

/*
===========
FanLength
===========
*/
int FanLength (int starttri, int startv)
{
	int		m1, m2, j, k;
	mtriangle_t	*last, *check;

	used[starttri] = 2;

	last = &triangles[starttri];

	stripverts[0] = last->vertindex[(startv)%3];
	stripverts[1] = last->vertindex[(startv+1)%3];
	stripverts[2] = last->vertindex[(startv+2)%3];

	striptris[0] = starttri;
	stripcount = 1;

	m1 = last->vertindex[(startv+0)%3];
	m2 = last->vertindex[(startv+2)%3];


	// look for a matching triangle
nexttri:
	for (j=starttri+1, check=&triangles[starttri+1] ; j<pheader->numtris ; j++, check++)
	{
		if (check->facesfront != last->facesfront)
			continue;
		for (k=0 ; k<3 ; k++)
		{
			if (check->vertindex[k] != m1)
				continue;
			if (check->vertindex[ (k+1)%3 ] != m2)
				continue;

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if (used[j])
				goto done;

			// the new edge
			m2 = check->vertindex[ (k+2)%3 ];

			stripverts[stripcount+2] = m2;
			striptris[stripcount] = j;
			stripcount++;

			used[j] = 2;
			goto nexttri;
		}
	}

done:
	// clear the temp used flags
	for (j=starttri+1 ; j<pheader->numtris ; j++)
		if (used[j] == 2)
			used[j] = 0;

	return stripcount;
}


/*
================
BuildTris

Generate a list of trifans or strips
for the model, which holds for all frames
================
*/
void BuildTris (void)
{
	int	i, j, k, startv, len, bestlen, besttype, type;
	float	s, t;
	int	bestverts[1024], besttris[1024];

	// build tristrips
	numorder = 0;
	numcommands = 0;
	memset (used, 0, sizeof(used));
	for (i=0 ; i<pheader->numtris ; i++)
	{
		// pick an unused triangle and start the trifan
		if (used[i])
			continue;

		bestlen = 0;
		for (type=0 ; type<2 ; type++)
//	type = 1;
		{
			for (startv=0 ; startv < 3 ; startv++)
			{
				len = (type == 1) ? StripLength (i, startv) : FanLength (i, startv);
				if (len > bestlen)
				{
					besttype = type;
					bestlen = len;
					for (j=0 ; j<bestlen+2 ; j++)
						bestverts[j] = stripverts[j];
					for (j=0 ; j<bestlen ; j++)
						besttris[j] = striptris[j];
				}
			}
		}

		// mark the tris on the best strip as used
		for (j=0 ; j<bestlen ; j++)
			used[besttris[j]] = 1;

		commands[numcommands++] = (besttype == 1) ? (bestlen+2) : -(bestlen+2);

		for (j=0 ; j<bestlen+2 ; j++)
		{
			// emit a vertex into the reorder buffer
			k = bestverts[j];
			vertexorder[numorder++] = k;

			// emit s/t coords into the commands stream
			s = stverts[k].s;
			t = stverts[k].t;
			if (!triangles[besttris[0]].facesfront && stverts[k].onseam)
				s += pheader->skinwidth / 2;	// on back side
			s = (s + 0.5) / pheader->skinwidth;
			t = (t + 0.5) / pheader->skinheight;

			*(float *)&commands[numcommands++] = s;
			*(float *)&commands[numcommands++] = t;
		}
	}

	commands[numcommands++] = 0;		// end of list marker

	//Con_DPrintf (1,"%3i tri %3i vert %3i cmd\n", pheader->numtris, numorder, numcommands);

	allverts += numorder;
	alltris += pheader->numtris;
}


/*
================
GL_MakeAliasModelDisplayLists
================

void GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr)
{
	int		i, j;
	int			*cmds;
	trivertx_t	*verts;

	aliasmodel = m;
	paliashdr = hdr;	// (aliashdr_t *)Mod_Extradata (m);

	// Tonik: don't cache anything, because it seems just as fast
	// (if not faster) to rebuild the tris instead of loading them from disk
	BuildTris ();		// trifans or lists

	// save the data out
	paliashdr->poseverts = numorder;

	cmds = Hunk_Alloc (numcommands * 4);
	paliashdr->commands = (byte *)cmds - (byte *)paliashdr;
	memcpy (cmds, commands, numcommands * 4);

	verts = Hunk_Alloc (paliashdr->numposes * paliashdr->poseverts 	* sizeof(trivertx_t) );
	paliashdr->posedata = (byte *)verts - (byte *)paliashdr;

	for (i=0 ; i<paliashdr->numposes ; i++)
		for (j=0 ; j<numorder ; j++)
			*verts++ = poseverts[i][vertexorder[j]];
}
*/

/*
================
GL_MakeAliasModelDisplayLists
================
*/
void ScaleVerts (byte *original, vec3_t scaled)
{
   scaled[0] = (float) original[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
   scaled[1] = (float) original[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
   scaled[2] = (float) original[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];
}

void GL_TriPrecalc(aliashdr_t *paliashdr, mtriangle_t *tris, trivertx_t *verts);

void GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr)
{
	int		    i, j;
	int			*cmds;
	trivertx_t  *verts;
	extern cvar_t gl_clip_muzzleflash;
#ifndef USEFAKEGL	
	mtriangle_t *tris;
#endif

	aliasmodel = m;
	paliashdr = hdr;   // (aliashdr_t *)Mod_Extradata (m);
#ifndef USEFAKEGL
//GLQuake stencil shadows : Rich Whitehouse-- start
	//First save the original vertex data for reference by our tris
	verts = Hunk_Alloc (paliashdr->numposes * paliashdr->numverts 
		* sizeof(trivertx_t) );
	paliashdr->baseposedata = (byte *)verts - (byte *)paliashdr;
	for (i=0 ; i<paliashdr->numposes ; i++)
	{
		for (j=0 ; j<paliashdr->numverts ; j++)
		{
			*verts++ = poseverts[i][j];	
		}
	}
	
	//Now save the triangles as we will need them for shadow volumes
	tris = Hunk_Alloc (paliashdr->numtris * sizeof(mtriangle_t) );
	paliashdr->triangles = (byte *)tris - (byte *)paliashdr;
	memcpy(tris, triangles, paliashdr->numtris * sizeof(mtriangle_t));

	//Calculate some stuff for the tris ahead of time
	GL_TriPrecalc(paliashdr, tris, verts);
//GLQuake stencil shadows : Rich Whitehouse-- end
#endif
	BuildTris ();      // trifans or lists

	// save the data out
	paliashdr->poseverts = numorder;

	cmds = Hunk_AllocName (numcommands * 4,"paliashdr->commands");
	paliashdr->commands = (byte *)cmds - (byte *)paliashdr;
	memcpy (cmds, commands, numcommands * 4);

	verts = Hunk_AllocName (paliashdr->numposes * paliashdr->poseverts * sizeof(trivertx_t),"trivertx_t" );

	paliashdr->posedata = (byte *)verts - (byte *)paliashdr;

	for (i=0 ; i<paliashdr->numposes ; i++)
		for (j=0 ; j<numorder ; j++)
			*verts++ = poseverts[i][vertexorder[j]];

	// MH: here's a nasty hack for you!
	// code for elimination of muzzleflashes on viewmodels.
	// is it a view model???  the only other model that normally begins with "v_" is the
	// vore spike, so we also check the number of frames
	if ((gl_clip_muzzleflash.value)&&((m->name[6] == 'v' || m->name[6] == 'V') && m->name[7] == '_' && hdr->numframes > 2))
	{
	  // scaled versions of the verts (need to prescale for comparison)
	  vec3_t scaledf0;
	  vec3_t scaledf1;

	  // difference in front to back movement
	  float vdiff;

	  // true if the vert is a muzzleflash
	  qboolean *nodraw;

	  // get pointers to the verts
	  trivertx_t *vertsf0 = (trivertx_t *) ((byte *) hdr + hdr->posedata);
	  trivertx_t *vertsfi = (trivertx_t *) ((byte *) hdr + hdr->posedata);

	  // set up the nodraw buffer
	  nodraw = Q_malloc (numorder * sizeof (qboolean));

	  // setthese pointers to the 0th and 1st frames
	  vertsf0 += hdr->frames[0].firstpose * hdr->poseverts;
	  vertsfi += hdr->frames[1].firstpose * hdr->poseverts;

	  // now go through them and compare.  we expect that (a) the animation is sensible and there's no major
	  // difference between the 2 frames to be expected, and (b) any verts that do exhibit a major difference
	  // can be assumed to belong to the muzzleflash
	  for (j = 0; j < numorder; j++)
	  {
		 ScaleVerts (vertsf0->v, scaledf0);
		 ScaleVerts (vertsfi->v, scaledf1);

		 // get difference in front to back movement
		 vdiff = scaledf1[0] - scaledf0[0];

		 // if it's above a certain treshold, assume a muzzleflash and mark for nodraw
		 // 10 is the approx lowest range of visible front to back in a view model, so that seems reasonable to work with
		 if (vdiff > 10)
			nodraw[j] = true;
		 else nodraw[j] = false;

		 // next set of verts
		 vertsf0++;
		 vertsfi++;
	  }

	  // copy the relevant verts from the first frame to every other frame
	  for (i = 1; i < paliashdr->numframes; i++)
	  {
		 // get pointers to the verts again
		 vertsf0 = (trivertx_t *) ((byte *) hdr + hdr->posedata);
		 vertsfi = (trivertx_t *) ((byte *) hdr + hdr->posedata);

		 // set these pointers to the 0th and i'th frames
		 vertsf0 += hdr->frames[0].firstpose * hdr->poseverts;
		 vertsfi += hdr->frames[i].firstpose * hdr->poseverts;

		 for (j = 0; j < numorder; j++)
		 {
			// copy the verts from frame 0
			if (nodraw[j])
			{
			   vertsfi->v[0] = vertsf0->v[0];
			   vertsfi->v[1] = vertsf0->v[1];
			   vertsfi->v[2] = vertsf0->v[2];
			}

			// next set of verts
			vertsf0++;
			vertsfi++;
		 }
	  }

	  // release the nodraw buffer
	  Q_free (nodraw);
	}
} 

