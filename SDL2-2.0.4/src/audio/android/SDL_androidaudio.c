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

#if SDL_AUDIO_DRIVER_ANDROID

/* Output audio to Android */

#include "SDL_audio.h"
#include "../SDL_audio_c.h"
#include "SDL_androidaudio.h"

#include "../../core/android/SDL_android.h"
#include "SDL_hints.h"

#include <android/log.h>

static SDL_AudioDevice* audioDevice = NULL;

static int
AndroidAUD_OpenDevice(_THIS, void *handle, const char *devname, int iscapture)
{
    SDL_AudioFormat test_format;

    if (iscapture) {
        /* TODO: implement capture */
        return SDL_SetError("Capture not supported on Android");
    }

    if (audioDevice != NULL) {
        return SDL_SetError("Only one audio device at a time please!");
    }

    audioDevice = this;
    
    this->hidden = (struct SDL_PrivateAudioData *) SDL_calloc(1, (sizeof *this->hidden));
    if (this->hidden == NULL) {
        return SDL_OutOfMemory();
    }

    test_format = SDL_FirstAudioFormat(this->spec.format);
    while (test_format != 0) { /* no "UNKNOWN" constant */
        if ((test_format == AUDIO_U8) || (test_format == AUDIO_S16LSB)) {
            this->spec.format = test_format;
            break;
        }
        test_format = SDL_NextAudioFormat();
    }

    if (test_format == 0) {
        /* Didn't find a compatible format :( */
        return SDL_SetError("No compatible audio format!");
    }

    if (this->spec.channels > 1) {
        this->spec.channels = 2;
    } else {
        this->spec.channels = 1;
    }

    if (this->spec.freq < 8000) {
        this->spec.freq = 8000;
    }
    if (this->spec.freq > 48000) {
        this->spec.freq = 48000;
    }

    /* TODO: pass in/return a (Java) device ID, also whether we're opening for input or output */
    this->spec.samples = Android_JNI_OpenAudioDevice(this->spec.freq, this->spec.format == AUDIO_U8 ? 0 : 1, this->spec.channels, this->spec.samples);
    SDL_CalculateAudioSpec(&this->spec);

    if (this->spec.samples == 0) {
        /* Init failed? */
        return SDL_SetError("Java-side initialization failed!");
    }

    return 0;
}

static void
AndroidAUD_PlayDevice(_THIS)
{
    Android_JNI_WriteAudioBuffer();
}

static Uint8 *
AndroidAUD_GetDeviceBuf(_THIS)
{
    return Android_JNI_GetAudioBuffer();
}

static void
AndroidAUD_CloseDevice(_THIS)
{
    /* At this point SDL_CloseAudioDevice via close_audio_device took care of terminating the audio thread
       so it's safe to terminate the Java side buffer and AudioTrack
     */
    Android_JNI_CloseAudioDevice();

    if (audioDevice == this) {
        if (audioDevice->hidden != NULL) {
            SDL_free(this->hidden);
            this->hidden = NULL;
        }
        audioDevice = NULL;
    }
}

#ifdef ANDROID
#include <android/log.h>
#define posix_print(format, ...) __android_log_print(ANDROID_LOG_INFO, "SDL", format, ##__VA_ARGS__)
#elif defined(__APPLE__) // mac os x/ios
#define posix_print(format, ...) fprintf(stderr, format, ##__VA_ARGS__) 
#else
#define posix_print(format, ...)
#endif

static int require_xmit = 0;

static jboolean nativeFillAudio(JNIEnv* env, jarray buffer, SDL_bool is16Bit)
{
    SDL_AudioDevice* this = audioDevice;

	if (!require_xmit) {
		return JNI_FALSE;
	}

	// here, I want to reduce copy, but fail! Hope someone can do it.
	// jboolean isCopy = JNI_FALSE;
    // void* bufferPinned = (*env)->GetShortArrayElements(env, buffer, &isCopy);
	void* bufferPinned;
	int length;
	
	if (is16Bit) {
		bufferPinned = (*env)->GetShortArrayElements(env, (jshortArray)buffer, NULL);
		length = (*env)->GetArrayLength(env, (jshortArray)buffer) * 2;
	} else {
		bufferPinned = (*env)->GetByteArrayElements(env, (jbyteArray)buffer, NULL);
		length = (*env)->GetArrayLength(env, (jbyteArray)buffer);
	}

    /* Only do anything if audio is enabled and not paused */
    if (this->enabled && !this->paused) {
		static Uint32 last_ticks = 0;
		Uint32 now_ticks = SDL_GetTicks();
		if (now_ticks - last_ticks >= 5000) {
			posix_print("nativeFillAudio, 1, bufferPinned: %p, length: %i, enabled: %s, paused: %s\n", bufferPinned, length, this->enabled? "true": "false", this->paused? "true": "false");
			last_ticks = now_ticks;
		}
		// Generate the data
		SDL_LockMutex(this->mixer_lock);
		(*this->spec.callback)(this->spec.userdata, bufferPinned, length);
		SDL_UnlockMutex(this->mixer_lock);

    } else {
		posix_print("nativeFillAudio, 2, bufferPinned: %p, length: %i, enabled: %s, paused: %s\n", bufferPinned, length, this->enabled? "true": "false", this->paused? "true": "false");
		SDL_memset(bufferPinned, this->spec.silence, length);
	}

	(*env)->ReleaseByteArrayElements(env, buffer, bufferPinned, 0);
	(*env)->DeleteLocalRef(env, buffer);

	return JNI_TRUE;
}

/* The Audio callback */
JNIEXPORT jboolean JNICALL Java_org_libsdl_app_PlaybackAudioService_nativeFillByteAudio(JNIEnv* env, jclass jcls, jarray buffer)
{
	return nativeFillAudio(env, buffer, SDL_FALSE);
}

JNIEXPORT jboolean JNICALL Java_org_libsdl_app_PlaybackAudioService_nativeFillShortAudio(JNIEnv* env, jclass jcls, jarray buffer)
{
	return nativeFillAudio(env, buffer, SDL_TRUE);
}

static void AndroidAUD_XmitData(_THIS, int start)
{
	// if start = 0, it is called in nativeFillAudio, and is called by JavaVM.
	// Android_XmitAudio will call JavaVM, Combine these two will result in JavaVM nesting.
	// In order to avoid to nest, stop use 
	if (start) {
		Android_XmitAudio(JNI_TRUE);
	}
	require_xmit = start;
}

static int
AndroidAUD_Init(SDL_AudioDriverImpl * impl)
{
    /* Set the function pointers */
    impl->OpenDevice = AndroidAUD_OpenDevice;
    impl->PlayDevice = AndroidAUD_PlayDevice;
    impl->GetDeviceBuf = AndroidAUD_GetDeviceBuf;
	impl->XmitData = AndroidAUD_XmitData;
    impl->CloseDevice = AndroidAUD_CloseDevice;

    /* and the capabilities */
    impl->HasCaptureSupport = 0; /* TODO */
    impl->OnlyHasDefaultOutputDevice = 1;
    impl->OnlyHasDefaultInputDevice = 1;

	impl->ProvidesOwnCallbackThread = 1;

    return 1;   /* this audio target is available. */
}

AudioBootStrap ANDROIDAUD_bootstrap = {
    "android", "SDL Android audio driver", AndroidAUD_Init, 0
};

/* Pause (block) all non already paused audio devices by taking their mixer lock */
void AndroidAUD_PauseDevices(void)
{
    /* TODO: Handle multiple devices? */
	const char* hint = SDL_GetHint(SDL_HINT_BACKGROUND_AUDIO);

    struct SDL_PrivateAudioData *private;
    if(audioDevice != NULL && audioDevice->hidden != NULL) {
        private = (struct SDL_PrivateAudioData *) audioDevice->hidden;
        if (audioDevice->paused) {
            /* The device is already paused, leave it alone */
            private->resume = SDL_FALSE;
        }
        else if (hint && hint[0] == '0') {
            SDL_LockMutex(audioDevice->mixer_lock);
            audioDevice->paused = SDL_TRUE;
            private->resume = SDL_TRUE;
        }
    }
}

/* Resume (unblock) all non already paused audio devices by releasing their mixer lock */
void AndroidAUD_ResumeDevices(void)
{
    /* TODO: Handle multiple devices? */
    struct SDL_PrivateAudioData *private;
    if(audioDevice != NULL && audioDevice->hidden != NULL) {
        private = (struct SDL_PrivateAudioData *) audioDevice->hidden;
        if (private->resume) {
            audioDevice->paused = SDL_FALSE;
            private->resume = SDL_FALSE;
            SDL_UnlockMutex(audioDevice->mixer_lock);
        }
    }
}


#endif /* SDL_AUDIO_DRIVER_ANDROID */

/* vi: set ts=4 sw=4 expandtab: */

