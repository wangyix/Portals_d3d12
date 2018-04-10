#pragma once

#include "d3dUtil.h"
#include "Light.h"
#include "MathHelper.h"
#include "UploadBuffer.h"

struct ObjectConstants
{
  DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
  DirectX::XMFLOAT4X4 WorldInvTranspose = MathHelper::Identity4x4();
  DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
  UINT     MaterialIndex;
  UINT     ObjPad0;
  UINT     ObjPad1;
  UINT     ObjPad2;
};

struct PassConstants
{
  DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
  DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
  float ViewScale = 1.0f;
};

struct DirectionalLightData {
  XMFLOAT3 Strength = { 0.0f, 0.0f, 0.0f };
  float LightPad0;
  XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };
  float LightPad1;
};

struct FrameConstants
{
  DirectX::XMFLOAT4X4 PortalA = MathHelper::Identity4x4();
  DirectX::XMFLOAT4X4 PortalB = MathHelper::Identity4x4();
  DirectX::XMFLOAT3 ClipPlanePosition = { 0.0f, 0.0f, 0.0f };
  float PassPad0;
  DirectX::XMFLOAT3 ClipPlaneNormal = { 0.0f, 1.0f, 0.0f };
  float ClipPlaneOffset = 0.0f;
  DirectX::XMFLOAT3 AmbientLight = { 0.0f, 0.0f, 0.0f };
  float PassPad1;
  DirectionalLightData Lights[NUM_LIGHTS];
};

struct PhongMaterialData
{
  DirectX::XMFLOAT4 Diffuse = { 0.0f, 0.0f, 0.0f, 0.0f };
  DirectX::XMFLOAT4 Specular = { 0.0f, 0.0f, 0.0f, 0.0f };
  UINT DiffuseMapIndex = 0;
  UINT MatPad0;;
  UINT MatPad1;
  UINT MatPad2;
};

struct Vertex
{
  DirectX::XMFLOAT3 Pos;
  DirectX::XMFLOAT3 Normal;
  DirectX::XMFLOAT2 TexC;
};

// Stores the resources needed for the CPU to build the command lists
// for a frame.  
struct FrameResource
{
public:

  FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);
  FrameResource(const FrameResource& rhs) = delete;
  FrameResource& operator=(const FrameResource& rhs) = delete;
  ~FrameResource();

  // We cannot reset the allocator until the GPU is done processing the commands.
  // So each frame needs their own allocator.
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

  // We cannot update a cbuffer until the GPU is done processing the commands
  // that reference it.  So each frame needs their own cbuffers.
  std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
  std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
  std::unique_ptr<UploadBuffer<FrameConstants>> FrameCB = nullptr;
  
  std::unique_ptr<UploadBuffer<PhongMaterialData>> MaterialBuffer = nullptr;

  // Fence value to mark commands up to this fence point.  This lets us
  // check if these frame resources are still in use by the GPU.
  UINT64 Fence = 0;
};