#include "../SDL_ble_c.h"

void WIN_ScanPeripherals(const char* uuid)
{
	// prepare dummy peripheral for debug.

	const char* name = NULL;
	void* cookie = (void*)0x1000;
	SDL_BlePeripheral* peripheral = discover_peripheral_uh(cookie, name);
	if (!peripheral->cookie) {
		peripheral->cookie = cookie;
		peripheral->mac_addr[0] = 0x12;
		peripheral->mac_addr[1] = 0x34;
		peripheral->mac_addr[2] = 0x56;
		peripheral->mac_addr[3] = 0x78;
		peripheral->mac_addr[4] = 0x9a;
		peripheral->mac_addr[5] = 0x10;
		// peripheral->uuid;
		// peripheral->manufacturer_data_len;
		// peripheral->manufacturer_data;
	}
	discover_peripheral_bh(peripheral, -79);

	name = "Thermometer";
	cookie = (void*)0x1001;
	peripheral = discover_peripheral_uh(cookie, name);
	if (!peripheral->cookie) {
		peripheral->cookie = cookie;
		peripheral->mac_addr[0] = 0x12;
		peripheral->mac_addr[1] = 0x34;
		peripheral->mac_addr[2] = 0x56;
		peripheral->mac_addr[3] = 0x78;
		peripheral->mac_addr[4] = 0x9a;
		peripheral->mac_addr[5] = 0x11;
		// peripheral->uuid;
		// peripheral->manufacturer_data_len;
		// peripheral->manufacturer_data;
	}
	discover_peripheral_bh(peripheral, 81);

	name = "Sesame_HTS";
	cookie = (void*)0x1002;
	peripheral = discover_peripheral_uh(cookie, name);
	if (!peripheral->cookie) {
		peripheral->cookie = cookie;
		peripheral->mac_addr[0] = 0x12;
		peripheral->mac_addr[1] = 0x34;
		peripheral->mac_addr[2] = 0x56;
		peripheral->mac_addr[3] = 0x78;
		peripheral->mac_addr[4] = 0x9a;
		peripheral->mac_addr[5] = 0x12;
		// peripheral->uuid;
		// peripheral->manufacturer_data_len;
		// peripheral->manufacturer_data;
	}
	discover_peripheral_bh(peripheral, -82);

	name = "Three";
	cookie = (void*)0x1003;
	peripheral = discover_peripheral_uh(cookie, name);
	if (!peripheral->cookie) {
		peripheral->cookie = cookie;
		peripheral->mac_addr[0] = 0x12;
		peripheral->mac_addr[1] = 0x34;
		peripheral->mac_addr[2] = 0x56;
		peripheral->mac_addr[3] = 0x78;
		peripheral->mac_addr[4] = 0x9a;
		peripheral->mac_addr[5] = 0x13;
		// peripheral->uuid;
		// peripheral->manufacturer_data_len;
		// peripheral->manufacturer_data;
	}
	discover_peripheral_bh(peripheral, 83);
}

// Windows driver bootstrap functions
static int WIN_Available(void)
{
    return (1);
}

static SDL_MiniBle* WIN_CreateBle()
{
    SDL_MiniBle *ble;

    // Initialize all variables that we clean on shutdown
    ble = (SDL_MiniBle *)SDL_calloc(1, sizeof(SDL_MiniBle));
    if (!ble) {
        SDL_OutOfMemory();
        return (0);
    }

    // Set the function pointers
	ble->ScanPeripherals = WIN_ScanPeripherals;

    return ble;
}


BleBootStrap WINDOWS_ble = {
    WIN_Available, WIN_CreateBle
};

