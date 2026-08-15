#pragma once

// Keep the binary compatible with the Windows 10/11 baseline documented by
// the project instead of silently targeting the newest installed SDK.
#ifndef WINVER
#define WINVER 0x0A00
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <SDKDDKVer.h>
