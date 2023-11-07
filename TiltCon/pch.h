#pragma once
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.h>
#include <winrt/windows.devices.bluetooth.h>
#include <winrt/windows.devices.enumeration.h>
#include <winrt/windows.devices.bluetooth.advertisement.h>
#include <winrt/windows.devices.bluetooth.genericattributeprofile.h>
#include <winrt/windows.storage.streams.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::System;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Storage::Streams;

#include <string>
#include <iostream>

using namespace std;

#define IN
#define OUT
#define ARRAYSIZE(a) (sizeof(a)/sizeof(a[0]))
typedef void* PVOID;
typedef uint32_t ULONG;
typedef uint8_t BYTE;
typedef BYTE* PBYTE;

