#include "Common.hlsl"

#ifndef FOG_START
#define FOG_START 20.0f
#endif
#ifndef FOG_RANGE
#define FOG_RANGE 80.0f //500.0f
#endif
#ifndef FOG_COLOR
#define FOG_COLOR float3(0.7f, 0.7f, 0.7f)
#endif

#ifndef PORTAL_TEX_RAD_RATIO
// Ratio of portal texture width to portal hole diameter.
#define PORTAL_TEX_RAD_RATIO 1.0f
#endif

#define PORTAL_Z_EPSILON 0.001f

float3 ComputeDirectionalLight(
    DirectionalLight L, float3 Diffuse, float4 Specular, float3 normal, float3 toEyeDir) {
  float diffuseFactor = max(dot(-L.Direction, normal), 0.0f);
  float3 toLightReflectedDir = reflect(L.Direction, normal);
  float specFactor = pow(max(dot(toLightReflectedDir, toEyeDir), 0.0f), Specular.w);
  return L.Strength * (diffuseFactor * Diffuse + specFactor * Specular.rgb);
}

struct VertexIn {
  float3 PosL    : POSITION;
  float3 NormalL : NORMAL;
  float2 TexC    : TEXCOORD;
};

struct VertexOut {
  float4 PosH         : SV_POSITION;
  float3 PosW         : POSITION;
  float3 NormalW      : NORMAL;
  float2 TexC         : TEXCOORD0;
#ifdef DRAW_PORTALS
  float3 PosPA	      : POSITION1;		// position in portalA space
  float3 PosPB	      : POSITION2;		// position in portalB space
#endif
};

VertexOut VS(VertexIn vin) {
  VertexOut vout;
  
  // Transform to world space.
  float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
  vout.PosW = posW.xyz;
  float3 normalW = mul(vin.NormalL, (float3x3)gWorldInvTranspose);
  vout.NormalW = mul(normalW, (float3x3)gLightWorldInvTranspose); // normalize this?
  
  // Transform to homogeneous clip space.
  vout.PosH = mul(posW, gViewProj);
  
  // Output vertex attributes for interpolation across triangle.
  vout.TexC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform).xy;

  // Transform from world space to portal space of both portals.
  // In portal space, the portal is centered at origin in XY plane with radius 1. The Z-axis
  // of portal space is not scaled relative to world space, while the X,Y axes are scaled by the
  // reciprocal of the portal's world-space hole radius.
#ifdef DRAW_PORTALS
  vout.PosPA = mul(posW, gPortalA).xyz;
  vout.PosPB = mul(posW, gPortalB).xyz;
#endif

  return vout;
}

float4 PS(VertexOut pin) : SV_TARGET {  
#ifdef CLIP_PLANE
  clip(dot(pin.PosW - gClipPlanePosition, gClipPlaneNormal) - gClipPlaneOffset);
#endif

  // Interpolating normal can unnormalize it, so renormalize it.
  pin.NormalW = normalize(pin.NormalW);

  float3 toEyeDirW = gEyePosW - pin.PosW;
  float distToEye = length(toEyeDirW);
  toEyeDirW /= distToEye;

  MaterialData matData = gMaterialData[gMaterialIndex];
  float3 diffuseAlbedo = (matData.Diffuse *
      gTextureMaps[matData.DiffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC)).rgb;

  float3 result = gAmbientLight * diffuseAlbedo;

  [unroll]
  for (int i = 0; i < NUM_LIGHTS; ++i) {
    result += ComputeDirectionalLight(
        gLights[i], diffuseAlbedo, matData.Specular, pin.NormalW, toEyeDirW);
  }
  
#ifdef DRAW_PORTALS
  // |portalDiffuse| will be alpha-blended with result from lighting calculations.
  float4 portalDiffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);

  if (abs(pin.PosPA.z) <= PORTAL_Z_EPSILON) {
    // Drop pixel if it's within the portal hole
    clip(dot(pin.PosPA.xy, pin.PosPA.xy) - 1.0f);

    // Calculate texcoord of this point for the portal texture
    // x:[-PORTAL_TEX_RAD_RATIO, PORTAL_TEX_RAD_RATIO] -> u:[1, 0]
    // y:[-PORTAL_TEX_RAD_RATIO, PORTAL_TEX_RAD_RATIO] -> v:[1, 0]
    float2 texC = 0.5f * (1.0f - pin.PosPA.xy / PORTAL_TEX_RAD_RATIO);
    portalDiffuse += gPortalAMap.Sample(gsamAnisotropicBlackBorder, texC);
  }

  if (abs(pin.PosPB.z) <= PORTAL_Z_EPSILON) {
    // Drop pixel if it's within the portal hole
    clip(dot(pin.PosPB.xy, pin.PosPB.xy) - 1.0f);

    // Calculate texcoord of this point for the portal texture
    // x:[-PORTAL_TEX_RAD_RATIO, PORTAL_TEX_RAD_RATIO] -> u:[1, 0]
    // y:[-PORTAL_TEX_RAD_RATIO, PORTAL_TEX_RAD_RATIO] -> v:[1, 0]
    float2 texC = 0.5f * (1.0f - pin.PosPB.xy / PORTAL_TEX_RAD_RATIO);
    portalDiffuse += gPortalBMap.Sample(gsamAnisotropicBlackBorder, texC);
  }

  result = lerp(result, portalDiffuse.rgb, portalDiffuse.a);
#endif
  
  // Blend result with fog color.
  float fogS = saturate((distToEye * gDistDilation - FOG_START) / FOG_RANGE);
  result = lerp(result, FOG_COLOR, fogS);
  
  return float4(result, 1.0f);
}