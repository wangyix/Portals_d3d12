#include "Common.hlsl"

#ifndef OUT_COLOR
#define OUT_COLOR float3(1.0f, 0.0f, 0.0f)
#endif

struct VertexIn {
  float3 PosL    : POSITION;
  float3 NormalL : NORMAL;
  float2 TexC    : TEXCOORD;
};

struct VertexOut {
  float4 PosH    : SV_POSITION;
  float3 PosW    : POSITION;    // Used for clipping
};

VertexOut VS(VertexIn vin) {
  VertexOut vout;

  // Transform to world space.
  float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
  vout.PosW = posW.xyz;
  
  // Transform to homogeneous clip space.
  vout.PosH = mul(posW, gViewProj);

#ifdef CLEAR_DEPTH
  // Set depth to max depth (1.0 after perspective divide).
  vout.PosH.z = vout.PosH.w;
#endif
  
  return vout;
}

float4 PS(VertexOut pin) : SV_TARGET {
#ifdef CLIP_PLANE
  clip(dot(pin.PosW - gClipPlanePosition, gClipPlaneNormal) - gClipPlaneOffset);
#endif
  return float4(OUT_COLOR, 1.0f);
}