// SPDX-License-Identifier: MIT

#pragma once

#include <string>

// Name of the processor, empty if unknown
std::string SystemCpuName();

// Name of the graphics card the renderer runs on, empty if unknown. Call after sg_setup.
std::string SystemGpuName();

// Physical cores of the fastest kind, without hyper-threads and efficiency cores. Zero if unknown.
int SystemPerformanceCores();
