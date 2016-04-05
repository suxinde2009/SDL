/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#ifdef SDL_FILESYSTEM_ANDROID

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* System dependent filesystem routines                                */

#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include "SDL_error.h"
#include "SDL_filesystem.h"
#include "SDL_system.h"


char *
SDL_GetBasePath(void)
{
    /* The current working directory is / on Android */
    return NULL;
}

char *
SDL_GetPrefPath(const char *org, const char *app)
{
    const char *path = SDL_AndroidGetInternalStoragePath();
    if (path) {
        size_t pathlen = SDL_strlen(path)+2;
        char *fullpath = (char *)SDL_malloc(pathlen);
        if (!fullpath) {
            SDL_OutOfMemory();
            return NULL;
        }
        SDL_snprintf(fullpath, pathlen, "%s/", path);
        return fullpath;
    }
    return NULL;
}

SDL_bool SDL_IsDirectory(const char* dir)
{
	if (*dir == '/') {
		struct stat dir_stat;
		if (stat(dir, &dir_stat) == -1) {
			return SDL_FALSE;
		}
		return S_ISDIR(dir_stat.st_mode)? SDL_TRUE: SDL_FALSE;
	}

	// Try from the asset system
	return Android_JNI_IsDirectory(dir);
}

SDL_bool SDL_IsFile(const char* file)
{
	if (*file == '/') {
		struct stat st;
		return stat(file, &st) != -1? SDL_TRUE: SDL_FALSE;
	}

	// Try from the asset system
	return Android_JNI_IsFile(file);
}

#endif /* SDL_FILESYSTEM_ANDROID */

/* vi: set ts=4 sw=4 expandtab: */
