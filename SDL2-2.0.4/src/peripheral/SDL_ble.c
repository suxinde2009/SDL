#include "../SDL_internal.h"
#include "SDL_video.h"
#include "SDL_timer.h"
#include "SDL_ble_c.h"

static BleBootStrap *bootstrap[] = {
#if __WIN32__
    &WINDOWS_ble,
#endif
#if __IPHONEOS__
    &UIKIT_ble,
#endif
#if __ANDROID__
    &Android_ble,
#endif
    NULL
};

static SDL_MiniBle* _this = NULL;
static SDL_bool quit = SDL_FALSE;

#define MAX_BLE_PERIPHERALS			48

static SDL_BlePeripheral* ble_peripherals[MAX_BLE_PERIPHERALS];
static int valid_ble_peripherals = 0;
SDL_BleCallbacks* current_callbacks = NULL;

SDL_BlePeripheral* find_peripheral_from_cookie(const void* cookie)
{
	int at;
	SDL_BlePeripheral* peripheral = NULL;
	for (at = 0; at < valid_ble_peripherals; at ++) {
        peripheral = ble_peripherals[at];
        if (peripheral->cookie == cookie) {
            return peripheral;
        }
	}
	return NULL;
}

static SDL_BleService* find_service(SDL_BlePeripheral* peripheral, const char* uuid)
{
	int at;
	SDL_BleService* tmp;
	for (at = 0; at < peripheral->valid_services; at ++) {
		tmp = peripheral->services + at;
		if (!SDL_memcmp(tmp->uuid, uuid, SDL_strlen(uuid))) {
			return tmp;
		}
	}
	return NULL;
}

static SDL_BleCharacteristic* find_characteristic(SDL_BlePeripheral* device, const char* service_uuid, const char* uuid)
{
	SDL_BleService* service = find_service(device, service_uuid);

	int at;
	SDL_BleCharacteristic* tmp;
	for (at = 0; at < service->valid_characteristics; at ++) {
        tmp = service->characteristics + at;
        if (!SDL_memcmp(tmp->uuid, uuid, SDL_strlen(uuid))) {
            return tmp;
        }
	}
	return NULL;
}

static void free_service(SDL_BleService* service)
{
    free_characteristics(service);
    if (service->uuid) {
        SDL_free(service->uuid);
    }
	service->cookie = NULL;
}

static void free_services(SDL_BlePeripheral* peripheral)
{
	if (peripheral->services) {
		int at;
		for (at = 0; at < peripheral->valid_services; at ++) {
			free_service(peripheral->services + at);
		}
		SDL_free(peripheral->services);
		peripheral->services = NULL;
		peripheral->valid_services = 0;
	}
}

SDL_BleCharacteristic* find_characteristic_from_cookie(const SDL_BlePeripheral* peripheral, const void* cookie)
{
	int at, at2;
	SDL_BleService* service;
	SDL_BleCharacteristic* tmp;
	for (at = 0; at < peripheral->valid_services; at ++) {
		service = peripheral->services + at;
		for (at2 = 0; at2 < service->valid_characteristics; at2 ++) {
			tmp = service->characteristics + at2;
			if (tmp->cookie == cookie) {
				return tmp;
			}
		}	
	}
	return NULL;
}

SDL_BlePeripheral* discover_peripheral_uh(const void* cookie, const char* name)
{
	size_t s;

    SDL_BlePeripheral* peripheral = find_peripheral_from_cookie(cookie);
	if (peripheral) {
		return peripheral;
	}

	peripheral = (SDL_BlePeripheral*)SDL_malloc(sizeof(SDL_BlePeripheral));
    SDL_memset(peripheral, 0, sizeof(SDL_BlePeripheral));
	ble_peripherals[valid_ble_peripherals ++] = peripheral;
    
	// field: name
	if (!name || name[0] == '\0') {
		s = 0;
	} else {
		s = SDL_strlen(name);
	}
	peripheral->name = (char*)SDL_malloc(s + 1);
	if (s) {
		SDL_memcpy(peripheral->name, name, s);
	}
	peripheral->name[s] = '\0';

	return peripheral;
}

void discover_peripheral_bh(SDL_BlePeripheral* peripheral, int rssi)
{
	peripheral->rssi = rssi;
	peripheral->receive_advertisement = SDL_GetTicks();

	if (current_callbacks && current_callbacks->discover_peripheral) {
		current_callbacks->discover_peripheral(peripheral);
	}
}

void disconnect_peripheral_bh(SDL_BlePeripheral* peripheral, int error)
{
	if (!peripheral) {
		// when release a connected peripheral, miniBle maybe send null peripheral into it.
		return;
	}
	// memory data of service and characteristic are relative of one connection.
	// since this connection is disconnect, this data is invalid. release these data.
	free_services(peripheral);
	if (current_callbacks && current_callbacks->disconnect_peripheral) {
        current_callbacks->disconnect_peripheral(peripheral, error);
    }
    if (error) {
        // except disconnect, thank lost.
        SDL_BleReleasePeripheral(peripheral);
    }
}

void free_characteristics(SDL_BleService* service)
{
    if (service->characteristics) {
		int at;
        for (at = 0; at < service->valid_characteristics; at ++) {
            SDL_BleCharacteristic* characteristic = service->characteristics + at;
            SDL_free(characteristic->uuid);
            characteristic->cookie = NULL;
        }
        SDL_free(service->characteristics);
        service->characteristics = NULL;
        service->valid_characteristics = 0;
    }
}

void discover_services_uh(SDL_BlePeripheral* peripheral, int services)
{
	free_services(peripheral);
	peripheral->valid_services = services;

	if (peripheral->valid_services) {
		peripheral->services = (SDL_BleService*)SDL_malloc(peripheral->valid_services * sizeof(SDL_BleService));
		SDL_memset(peripheral->services, 0, peripheral->valid_services * sizeof(SDL_BleService));
	}
}

void discover_services_bh(SDL_BlePeripheral* peripheral, SDL_BleService* service, const char* uuid, void* cookie)
{
	size_t s = SDL_strlen(uuid);
	service->uuid = (char*)SDL_malloc(s + 1);
	SDL_memcpy(service->uuid, uuid, s);
	service->uuid[s] = '\0';
	service->cookie = cookie;
}

SDL_BleService* discover_characteristics_uh(SDL_BlePeripheral* peripheral, const char* service_uuid, int characteristics)
{
	SDL_BleService* service = SDL_BleFindService(peripheral, service_uuid);
	if (!service) {
		return NULL;
	}

    free_characteristics(service);
	service->valid_characteristics = characteristics;

	service->characteristics = (SDL_BleCharacteristic*)SDL_malloc(service->valid_characteristics * sizeof(SDL_BleCharacteristic));
	SDL_memset(service->characteristics, 0, service->valid_characteristics * sizeof(SDL_BleCharacteristic));

    return service;
}

void discover_characteristics_bh(SDL_BlePeripheral* peripheral, SDL_BleService* service, SDL_BleCharacteristic* characteristic, const char* uuid, void* cookie)
{
	size_t s = SDL_strlen(uuid);
	characteristic->uuid = (char*)SDL_malloc(s + 1);
	SDL_memcpy(characteristic->uuid, uuid, s);
	characteristic->uuid[s] = '\0';
	characteristic->service = service;
	characteristic->cookie = cookie;
	characteristic->properties = 0;
	// after it, miniBle should fill field: properties.
}

void release_peripherals()
{
	while (valid_ble_peripherals) {
		SDL_BlePeripheral* device = ble_peripherals[0];
		SDL_BleReleasePeripheral(device);
	}

	// valid_ble_peripherals = NULL;
	// ble = NULL;
}

void SDL_BleSetCallbacks(SDL_BleCallbacks* callbacks)
{
	current_callbacks = callbacks;
}

void SDL_BleScanPeripherals(const char* uuid)
{
	if (quit) {
		// SDL_PeripheralQuit will call SDL_BleReleasePeripheral.
		// if this peripheral is connected, app maybe call re-scan to keep discover. and discover maybe result to connect again.

		// --disconnect a peripheral
		// --rescan   <--of course, SDL don't want app to call it, but cannot avoid.
		// --discover a peripheral  <---here exists run after. once it is first, will result serious result and must avoid it.
		// --release peripheral memory.
		return;
	}

	if (_this->ScanPeripherals) {
		_this->ScanPeripherals(uuid);
	}
}

void SDL_BleStopScanPeripherals()
{
    if (_this->StopScanPeripherals) {
		_this->StopScanPeripherals();
	}
}

SDL_BleService* SDL_BleFindService(SDL_BlePeripheral* peripheral, const char* uuid)
{
	return find_service(peripheral, uuid);
}

SDL_BleCharacteristic* SDL_BleFindCharacteristic(SDL_BlePeripheral* peripheral, const char* service_uuid, const char* uuid)
{
	return find_characteristic(peripheral, service_uuid, uuid);
}

void SDL_BleReleasePeripheral(SDL_BlePeripheral* peripheral)
{
    int at;
    for (at = 0; at < valid_ble_peripherals; at ++) {
        SDL_BlePeripheral* tmp = ble_peripherals[at];
        if (tmp == peripheral) {
            break;
        }
    }
    if (at == valid_ble_peripherals) {
        return;
    }

    // if this peripheral is connected, disconnect first!
	if ((_this->IsConnected && _this->IsConnected(peripheral))) {
        // user may quit app directly. in this case, state return CBPeripheralStateDisconnected as if it is connecting.
        // It require that connected peripherl should disconnect before release, except quit.
        // In order to safe, don't let app call SDL_BleReleasePeripheral directly.
        // here, don't call SDL_BleDisconnectPeripheral, only call app-disocnnect to let app to right state.
        if (current_callbacks && current_callbacks->disconnect_peripheral) {
            current_callbacks->disconnect_peripheral(peripheral, 0);
        }
    }
    
    if (current_callbacks && current_callbacks->release_peripheral) {
        current_callbacks->release_peripheral(peripheral);
    }
    
    if (peripheral->name) {
        SDL_free(peripheral->name);
        peripheral->name = NULL;
    }
    if (peripheral->uuid) {
        SDL_free(peripheral->uuid);
        peripheral->uuid = NULL;
    }
	if (peripheral->manufacturer_data) {
        SDL_free(peripheral->manufacturer_data);
        peripheral->manufacturer_data = NULL;
		peripheral->manufacturer_data_len = 0;
    }
	free_services(peripheral);
    
    if (_this->ReleaseCookie) {
		_this->ReleaseCookie(peripheral);
	}
    peripheral->cookie = NULL;
    
	SDL_free(peripheral);
    if (at < valid_ble_peripherals - 1) {
        SDL_memcpy(ble_peripherals + at, ble_peripherals + (at + 1), (valid_ble_peripherals - at - 1) * sizeof(SDL_BlePeripheral*));
    }
	ble_peripherals[valid_ble_peripherals - 1] = NULL;
    
    valid_ble_peripherals --;
}

void SDL_BleStartAdvertise()
{
	if (_this->StartAdvertise) {
		_this->StartAdvertise();
	}
}

void SDL_BleConnectPeripheral(SDL_BlePeripheral* peripheral)
{
	if (_this->ConnectPeripheral) {
		_this->ConnectPeripheral(peripheral);
	}
    return;
}

void SDL_BleDisconnectPeripheral(SDL_BlePeripheral* peripheral)
{
	if (_this->DisconnectPeripheral) {
		_this->DisconnectPeripheral(peripheral);
	}
	return;
}

void SDL_BleGetServices(SDL_BlePeripheral* peripheral)
{
	if (_this->GetServices) {
		_this->GetServices(peripheral);
	}
}

void SDL_BleGetCharacteristics(SDL_BlePeripheral* peripheral, SDL_BleService* service)
{
	if (_this->GetCharacteristics) {
		_this->GetCharacteristics(peripheral, service);
	}
}

void SDL_BleReadCharacteristic(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic)
{
	if (_this->ReadCharacteristic) {
		_this->ReadCharacteristic(peripheral, characteristic);
	}
}

void SDL_BleSetNotify(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic)
{
	if (_this->SetNotify) {
		_this->SetNotify(peripheral, characteristic);
	}
}

void SDL_BleWriteCharacteristic(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, const uint8_t* data, int size)
{
	if (_this->WriteCharacteristic) {
		_this->WriteCharacteristic(peripheral, characteristic, data, size);
	}
}

void SDL_BleDiscoverDescriptors(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic)
{
	if (_this->DiscoverDescriptors) {
		_this->DiscoverDescriptors(peripheral, characteristic);
	}
}

int SDL_BleAuthorizationStatus()
{
	if (_this->AuthorizationStatus) {
		return _this->AuthorizationStatus();
	}
	return 0;
}

void SDL_PeripheralInit()
{
	SDL_MiniBle* ble = NULL;
	int i;

	for (i = 0; bootstrap[i]; ++ i) {
		if (bootstrap[i]->available()) {
			ble = bootstrap[i]->create();
			break;
		}
	}

	SDL_memset(ble_peripherals, 0, sizeof(ble_peripherals));

	_this = ble;
}

void SDL_PeripheralQuit()
{
	quit = SDL_TRUE;
	release_peripherals();

	SDL_free(_this);
	_this = NULL;
}