/*****************************************************************************
 * aout_sdl.c : audio sdl functions library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: aout.c,v 1.2 2002/08/07 21:36:56 massiot Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Pierre Baillet <oct@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <string.h>                                            /* strerror() */
#include <unistd.h>                                      /* write(), close() */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include "aout_internal.h"

#include SDL_INCLUDE_FILE

#define DEFAULT_FRAME_SIZE 2048

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int     SetFormat   ( aout_instance_t * );
static void    Play        ( aout_instance_t *, aout_buffer_t * );

static void    SDLCallback ( void *, Uint8 *, int );

/*****************************************************************************
 * OpenAudio: open the audio device
 *****************************************************************************/
int E_(OpenAudio) ( aout_instance_t *p_aout )
{
    if( SDL_WasInit( SDL_INIT_AUDIO ) != 0 )
    {
        return( 1 );
    }

    p_aout->output.pf_setformat = SetFormat;
    p_aout->output.pf_play = Play;

    /* Initialize library */
    if( SDL_Init( SDL_INIT_AUDIO
#ifndef WIN32
    /* Win32 SDL implementation doesn't support SDL_INIT_EVENTTHREAD yet*/
                | SDL_INIT_EVENTTHREAD
#endif
#ifdef DEBUG
    /* In debug mode you may want vlc to dump a core instead of staying
     * stuck */
                | SDL_INIT_NOPARACHUTE
#endif
                ) < 0 )
    {
        msg_Err( p_aout, "cannot initialize SDL (%s)", SDL_GetError() );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * SetFormat: reset the audio device and sets its format
 *****************************************************************************/
static int SetFormat( aout_instance_t *p_aout )
{
    /* TODO: finish and clean this */
    SDL_AudioSpec desired;

    desired.freq       = p_aout->output.output.i_rate;
    desired.format     = AUDIO_S16SYS;
    desired.channels   = p_aout->output.output.i_channels;
    desired.callback   = SDLCallback;
    desired.userdata   = p_aout;
    desired.samples    = DEFAULT_FRAME_SIZE;

    /* Open the sound device - FIXME : get the "natural" paramaters */
    if( SDL_OpenAudio( &desired, NULL ) < 0 )
    {
        return -1;
    }

    p_aout->output.output.i_format = AOUT_FMT_S16_NE;
    p_aout->output.i_nb_samples = DEFAULT_FRAME_SIZE;

    SDL_PauseAudio( 0 );

    return 0;
}

/*****************************************************************************
 * Play: play a sound samples buffer
 *****************************************************************************/
static void Play( aout_instance_t * p_aout, aout_buffer_t * p_buffer )
{
    SDL_LockAudio();                                     /* Stop callbacking */

    aout_FifoPush( p_aout, &p_aout->output.fifo, p_buffer );

    SDL_UnlockAudio();                                  /* go on callbacking */
}

/*****************************************************************************
 * CloseAudio: close the audio device
 *****************************************************************************/
void E_(CloseAudio) ( aout_instance_t *p_aout )
{
    SDL_PauseAudio( 1 );

    SDL_CloseAudio();

    SDL_QuitSubSystem( SDL_INIT_AUDIO );
}

/*****************************************************************************
 * SDLCallback: what to do once SDL has played sound samples
 *****************************************************************************/
static void SDLCallback( void * _p_aout, byte_t * p_stream, int i_len )
{
    aout_instance_t * p_aout = (aout_instance_t *)_p_aout;
    /* FIXME : take into account SDL latency instead of mdate() */
    aout_buffer_t * p_buffer = aout_OutputNextBuffer( p_aout, mdate() );

    if ( i_len != DEFAULT_FRAME_SIZE * sizeof(s16)
                    * p_aout->output.output.i_channels )
    {
        msg_Err( p_aout, "SDL doesn't know its buffer size (%d)", i_len );
    }

    if ( p_buffer != NULL )
    {
        p_aout->p_vlc->pf_memcpy( p_stream, p_buffer->p_buffer, i_len );
        aout_BufferFree( p_buffer );
    }
    else
    {
        memset( p_stream, 0, i_len );
    }
}

