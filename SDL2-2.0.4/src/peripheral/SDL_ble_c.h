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

#include "../SDL_internal.h"
#include "SDL_peripheral.h"

#ifndef _SDL_blit_c_h
#define _SDL_blit_c_h

typedef struct 
{
	void (*ScanPeripherals) (const char* uuid);
	void (*StopScanPeripherals) ();
	void (*StartAdvertise) ();
	void (*ConnectPeripheral) (const SDL_BlePeripheral* peripheral);
	void (*DisconnectPeripheral) (const SDL_BlePeripheral* peripheral);
	void (*GetServices) (const SDL_BlePeripheral* peripheral);
	void (*GetCharacteristics) (const SDL_BlePeripheral* peripheral, SDL_BleService* service);
	void (*ReadCharacteristic) (const SDL_BlePeripheral* peripheral, const SDL_BleCharacteristic* characteristic);
	void (*SetNotify) (const SDL_BlePeripheral* peripheral, const SDL_BleCharacteristic* characteristic);
	void (*WriteCharacteristic) (const SDL_BlePeripheral* peripheral, const SDL_BleCharacteristic* characteristic, const uint8_t* data, int size);
	void (*DiscoverDescriptors) (const SDL_BlePeripheral* peripheral, const SDL_BleCharacteristic* characteristic);
	int (*AuthorizationStatus) ();
	int (*IsConnected) (const SDL_BlePeripheral* peripheral);
	void (*ReleaseCookie) (const SDL_BlePeripheral* peripheral);
} SDL_MiniBle;

extern SDL_BleCallbacks* current_callbacks;

SDL_BlePeripheral* find_peripheral_from_cookie(const void* cookie);
SDL_BleCharacteristic* find_characteristic_from_cookie(const SDL_BlePeripheral* peripheral, const void* cookie);
SDL_BlePeripheral* discover_peripheral_uh(const void* cookie, const char* name);
void discover_peripheral_bh(SDL_BlePeripheral* peripheral, int rssi);
void disconnect_peripheral_bh(SDL_BlePeripheral* peripheral, int error);

void discover_services_uh(SDL_BlePeripheral* peripheral, int services);
void discover_services_bh(SDL_BlePeripheral* peripheral, SDL_BleService* service, const char* uuid, void* cookie);
SDL_BleService* discover_characteristics_uh(SDL_BlePeripheral* peripheral, const char* service_uuid, int characteristics);
void discover_characteristics_bh(SDL_BlePeripheral* peripheral, SDL_BleService* service, SDL_BleCharacteristic* characteristic, const char* uuid, void* cookie);

void free_characteristics(SDL_BleService* service);
void release_peripherals();

typedef struct
{
	int (*available) (void);
	SDL_MiniBle* (*create) ();
} BleBootStrap;

#if __WIN32__
extern BleBootStrap WINDOWS_ble;
#endif
#if __IPHONEOS__
extern BleBootStrap UIKIT_ble;
#endif
#if __ANDROID__
extern BleBootStrap Android_ble;
#endif

#endif