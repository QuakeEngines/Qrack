
/*
Copyright (C) 1996-2018.

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

#include "fmod.h"
#include "fmod_errors.h"
#include "quakedef.h"

#ifdef WIN32
static	HINSTANCE fmod_handle = NULL;
#else
static	void	*fmod_handle = NULL;
#endif

static FMUSIC_MODULE	*mod = NULL;
static FSOUND_STREAM	*stream = NULL;

static signed char		(F_API *qFSOUND_Init)(int, int, unsigned int);
static int				(F_API *qFSOUND_GetError)(void);
static void				(F_API *qFSOUND_Close)(void);
static signed char		(F_API *qFMUSIC_SetPaused)(FMUSIC_MODULE *, signed char);
static FMUSIC_MODULE   *(F_API *qFMUSIC_LoadSongEx)(const char *, int, int, unsigned int, const int *, int);
static signed char		(F_API *qFMUSIC_FreeSong)(FMUSIC_MODULE *);
static signed char		(F_API *qFMUSIC_PlaySong)(FMUSIC_MODULE *);
static signed char		(F_API *qFMUSIC_StopSong)(FMUSIC_MODULE *);
static signed char      (F_API *qFMUSIC_SetMasterVolume)(FMUSIC_MODULE *, int);
static void             (F_API *qFSOUND_SetSFXMasterVolume)(int volume);
static FSOUND_STREAM   *(F_API *qFSOUND_Stream_Open)(const char *, unsigned int, int, int);
static signed char		(F_API *qFSOUND_Stream_Close)(FSOUND_STREAM *);                           
static int				(F_API *qFSOUND_Stream_Play)(int channel, FSOUND_STREAM *);
static signed char		(F_API *qFSOUND_Stream_Stop)(FSOUND_STREAM *);

qboolean fmod_loaded;				//This means we found the .dll and it's loaded and initialized.
qboolean fmod_enabled	= false;	//This is a toggle for the end user to enable or disable the whole shabang.
qboolean modplaying		= false;
qboolean streamplaying	= false;

#ifdef _WIN32
#define FSOUND_GETFUNC(f, g) (qFSOUND_##f = (void *)GetProcAddress(fmod_handle, "_FSOUND_" #f #g))
#define FMUSIC_GETFUNC(f, g) (qFMUSIC_##f = (void *)GetProcAddress(fmod_handle, "_FMUSIC_" #f #g))
#else
#define FSOUND_GETFUNC(f, g) (qFSOUND_##f = (void *)dlsym(fmod_handle, "FSOUND_" #f))
#define FMUSIC_GETFUNC(f, g) (qFMUSIC_##f = (void *)dlsym(fmod_handle, "FMUSIC_" #f))
#endif

qboolean FMOD_LoadLibrary (void)
{
	fmod_loaded = false;

#ifdef _WIN32
	if (!(fmod_handle = LoadLibrary("fmod.dll")))
#else
	if (!(fmod_handle = dlopen("libfmod-3.73.so", RTLD_NOW)))
#endif
	{
		Con_Printf ("\x02" "FMOD module not found\n");
		goto fail;
	}

	FSOUND_GETFUNC(Init, @12);
	FSOUND_GETFUNC(GetError, @0);
	FSOUND_GETFUNC(Close, @0);
	FMUSIC_GETFUNC(LoadSongEx, @24);
	FMUSIC_GETFUNC(FreeSong, @4);
	FMUSIC_GETFUNC(PlaySong, @4);
	FMUSIC_GETFUNC(StopSong, @4);
	FMUSIC_GETFUNC(SetPaused, @8);
	FMUSIC_GETFUNC(SetMasterVolume, @8);
	FSOUND_GETFUNC(SetSFXMasterVolume, @4);
	FSOUND_GETFUNC(Stream_Open, @16);
	FSOUND_GETFUNC(Stream_Close, @4);
	FSOUND_GETFUNC(Stream_Play, @8);
	FSOUND_GETFUNC(Stream_Stop, @4);

	fmod_loaded = 	*qFSOUND_Init && 
					qFSOUND_GetError &&
					qFSOUND_Close &&
					qFMUSIC_LoadSongEx &&
					qFMUSIC_FreeSong &&
					qFMUSIC_PlaySong &&
					qFMUSIC_SetPaused &&
					qFMUSIC_StopSong &&
					qFMUSIC_SetMasterVolume &&
					qFSOUND_SetSFXMasterVolume &&
					qFSOUND_Stream_Open &&
					qFSOUND_Stream_Close &&
					qFSOUND_Stream_Play &&
					qFSOUND_Stream_Stop;

	if (!fmod_loaded)
	{		
		goto fail;
	}

	Con_Printf ("FMOD module initialized\n");
	return true;

fail:
	if (fmod_handle)
	{
		Con_Printf ("\x02" "FMOD module not initialized\n");
#ifdef _WIN32
		FreeLibrary (fmod_handle);
#else
		dlclose (fmod_handle);
#endif
		fmod_handle = NULL;
	}
	return false;
}


void FMOD_Close (void)
{
	if (fmod_loaded)
		qFSOUND_Close ();	
}

void FMOD_Stop_f (void)
{
	if (modplaying)
	{
		qFMUSIC_FreeSong (mod);
		mod = NULL;
	}
	modplaying = false;
}

void FMOD_Play_f (void)
{
	char	modname[1024], *buffer;
	int	mark;

	Q_strncpyz (modname, Cmd_Argv(1), sizeof(modname));

	if (modplaying)
		FMOD_Stop_f ();

	if (strlen(modname) < 3)
	{
		Con_Print ("Usage: playmod <filename.ext>");
		return;
	}

	mark = Hunk_LowMark ();

	if (!(buffer = (char *)COM_LoadHunkFile(modname)))
	{
		Con_Printf ("ERROR: Couldn't open %s\n", modname);
		return;
	}

	mod = qFMUSIC_LoadSongEx (buffer, 0, com_filesize, FSOUND_LOADMEMORY, NULL, 0);

	Hunk_FreeToLowMark (mark);

	if (!mod)
	{
		Con_Printf ("%s\n", FMOD_ErrorString(qFSOUND_GetError()));
		return;
	}

	modplaying = true;
	qFMUSIC_PlaySong (mod);
}

void FMOD_ChangeVolume (float value)
{
	static float fmod_vol;

	if (value == fmod_vol)
		return;

	if (mod)
		qFMUSIC_SetMasterVolume (mod, 256 * bound(0, value, 1));

	if (stream)
		qFSOUND_SetSFXMasterVolume (255 * bound(0, value, 1));
}

void FMOD_Stop_Stream_f (void)
{
	if (stream)
	{
	   qFSOUND_Stream_Stop(stream);
	   qFSOUND_Stream_Close(stream);
	   stream = NULL;
	}
	streamplaying = false;
}

void FMOD_Play_Stream_f (char *streamname)
{
	if (fmod_enabled == false)
		return;

	if (qFSOUND_Stream_Open)
	{
		stream = qFSOUND_Stream_Open (streamname,  FSOUND_NORMAL | FSOUND_LOOP_NORMAL, 0, 0);
		if (!(stream)) Con_Printf ("%s: %s\n", FMOD_ErrorString(qFSOUND_GetError()), streamname);
	}
	else 
	{
		Con_Printf ("%s: %s\n", FMOD_ErrorString(qFSOUND_GetError()), "qFSOUND_Stream_Open");
		return;
	}

	if (!stream)
	{
		return;
	}

	if (qFSOUND_Stream_Play (FSOUND_FREE, stream) == -1)
	{
		Con_Printf ("%s: %s\n", FMOD_ErrorString(qFSOUND_GetError()), "qFSOUND_Stream_Play");
		FMOD_Stop_Stream_f();
		return;
	}
	streamplaying = true;
	Con_Printf ("playing %s\n", streamname);
}

//todo fix this so we can use .ogg/mp3 in the sounds folder too
void FMOD_Play_Sample_f (void)
{
	char samplename[MAX_OSPATH];
	char sampledir[1024];

	Q_strncpyz (sampledir, com_basedir, sizeof(sampledir));
	Q_snprintfz (samplename, sizeof(samplename), "%s/%s", sampledir, Cmd_Argv(1));

	if (streamplaying)
		FMOD_Stop_Stream_f ();

	if (strlen(samplename) < 5)
	{
		Con_Print ("Usage: playsample <filename.ext>");
		return;
	}

	FMOD_Play_Stream_f (samplename);
}

void FMOD_PlayTrack (int track)
{
	char streamname[MAX_OSPATH];

	if (cls.timedemo) //Do not play the external music when doing a timedemo.
	{
		return;
	}

	if (streamplaying)
		FMOD_Stop_Stream_f ();

	Q_snprintfz (streamname, sizeof(streamname), "%s/music/track%02d.mp3",com_gamedir,track);
	
	if (!(COM_FindFile(streamname)))
		Q_snprintfz (streamname, sizeof(streamname), "%s/music/track%02d.ogg",com_gamedir,track);			

	FMOD_Play_Stream_f (streamname);
}

void FMOD_Enable_f (void)
{
	if (fmod_loaded == true)
	{
		fmod_enabled = !(fmod_enabled);

		if (fmod_enabled)
			Con_Printf ("Using FMOD to play music files: Enabled\n");
		else
			Con_Printf ("Using FMOD to play music files: Disabled\n");

		if (streamplaying)
			FMOD_Stop_Stream_f ();
	}
}

void FMOD_Init (void)
{
	if (!qFSOUND_Init(44100, 32, 0 ))
	{
		Con_Printf ("\x02" "Failed to load FMOD: %s\n", FMOD_ErrorString(qFSOUND_GetError()));
		qFSOUND_Close ();
		return;
	}

	Cmd_AddCommand ("stopmod", FMOD_Stop_f);
	Cmd_AddCommand ("playmod", FMOD_Play_f);
	Cmd_AddCommand ("stopsample", FMOD_Stop_Stream_f);
	Cmd_AddCommand ("playsample", FMOD_Play_Sample_f);
	Cmd_AddCommand ("fmod_music", FMOD_Enable_f);
}
