// SPDX-License-Identifier: MIT
// Single translation unit for the sokol implementations. The backend macro (SOKOL_D3D11, SOKOL_METAL or
// SOKOL_GLCORE) comes from CMake. On macOS this file is compiled as Objective-C.

#if defined( __linux__ ) && !defined( _POSIX_C_SOURCE )
#define _POSIX_C_SOURCE 200809L
#endif

#define SOKOL_IMPL
#define SOKOL_NO_ENTRY

#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_glue.h"
#include "sokol_log.h"
