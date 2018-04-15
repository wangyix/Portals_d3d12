#ifndef NUM_LIGHTS
#define NUM_LIGHTS 3
#endif

struct MaterialData {
  float4 Diffuse;
  float4 Specular;
  uint DiffuseMapIndex;
  uint MatPad0;
  uint MatPad1;
  uint MatPad2;
};

struct DirectionalLight {
  float3 Strength;
  float LightPad0;
  float3 Direction;
  float LightPad1;
};

Texture2D gPortalADiffuseMap : register(t0);
Texture2D gPortalBDiffuseMap : register(t1);

// An array of textures, which is only supported in shader model 5.1+.
// Unlike Texture2DArray, the textures in this array can be different sizes and formats, making it
// more flexible than texture arrays.
Texture2D gTextureMaps[2] : register(t2);

// Put in space1, so the texture array does not overlap with these resources.  
// The texture array will occupy registers t0, t1, ..., t3 in space0. 
StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);

SamplerState gsamAnisotropicWrap : register(s0);
SamplerState gsamAnisotropicBlackBorder : register(s1);

cbuffer cbPerObject : register(b0) {
  float4x4 gWorld;
  float4x4 gWorldInvTranspose;
  float4x4 gTexTransform;
  uint gMaterialIndex;
  uint gObjPad0;
  uint gObjPad1;
  uint gObjPad2;
};

cbuffer cbPass : register(b1) {
  float4x4 gViewProj;
  float3 gEyePosW;
  float gViewScale;
};

cbuffer cbClipPlane : register(b2) {
  float3 gClipPlanePosition;
  float gClipPlanePad0;
  float3 gClipPlaneNormal;
  float gClipPlaneOffset;
}

cbuffer cbFrame : register(b3) {
  float4x4 gPortalA;
  float4x4 gPortalB;
  float3 gAmbientLight;
  float gFramePad0;
  DirectionalLight gLights[NUM_LIGHTS];
};