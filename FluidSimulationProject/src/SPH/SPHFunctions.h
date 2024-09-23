#pragma once

Vec3i GetCell(Vec3f position, float maxInteractionDistance);
uint GetHash(Vec3i cell);
float SmoothingKernelConstant(float h);
float SmoothingKernelD0(float r, float maxInteractionDistance);
float SmoothingKernelD1(float r, float maxInteractionDistance);
float SmoothingKernelD2(float r, float maxInteractionDistance);
float Noise(float x);
Vec3f RandomDirection(float x);