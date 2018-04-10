#pragma once

#include <DirectXMath.h>

#define NUM_LIGHTS 3

using namespace DirectX;

// Replaces Light in d3dUtil.h
struct DirectionalLight {
  XMFLOAT3 Strength = { 0.0f, 0.0f, 0.0f };
  XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f }; // points down
};

// Replaces Material in d3dUtil.h
struct PhongMaterial {
  // Index into constant buffer corresponding to this material.
  int MatCBIndex = -1;

  // Index into SRV heap for diffuse texture.
  int DiffuseSrvHeapIndex = -1;

  // Dirty flag indicating the material has changed and we need to update the constant buffer.
  // Because we have a material constant buffer for each FrameResource, we have to apply the
  // update to each FrameResource.  Thus, when we modify a material we should set 
  // NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
  int NumFramesDirty = gNumFrameResources;

  XMFLOAT4 Diffuse = { 0.0f, 0.0f, 0.0f, 0.0f };
  XMFLOAT4 Specular = { 0.0f, 0.0f, 0.0f, 0.0f };
};
