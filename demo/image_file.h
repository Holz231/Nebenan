// SPDX-License-Identifier: MIT
// Reference images for the keyframe editor: uncompressed BMP and binary PPM files, as ffmpeg writes them from a video,
// for example ffmpeg -i clip.mp4 -vf fps=30 frames/%04d.bmp

#pragma once

#include <stdint.h>
#include <vector>

// Read an image into rows of RGBA pixels, the top row first. False if the file is missing or of another kind.
bool LoadImageFile( const char* path, int* width, int* height, std::vector<uint8_t>& pixels );
