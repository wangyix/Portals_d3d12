#pragma once

#include "d3dApp.h" // Include this first

#include "Camera.h"
#include "FrameResource.h"
#include "Room.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class PortalsApp : public D3DApp {
public:
  struct DirectionalLight {
    XMFLOAT3 Strength = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f }; // points down
  };

  struct PhongMaterial {
    XMFLOAT4 Diffuse = { 0.0f, 0.0f, 0.0f, 0.0f };
    XMFLOAT4 Specular = { 0.0f, 0.0f, 0.0f, 0.0f };
    int DiffuseSrvHeapIndex = -1;
  };

  struct RenderItem {
    XMFLOAT4X4 World = MathHelper::Identity4x4();
    XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

    // Dirty flag indicating the object data has changed and we need to update the constant buffer.
    // Because we have an object cbuffer for each FrameResource, we have to apply the
    // update to each FrameResource.  Thus, when we modify obect data we should set 
    // NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
    int NumFramesDirty = gNumFrameResources;

    // Index into GPU constant buffer corresponding to the ObjectCB for this render item.
    UINT ObjCBIndex = -1;

    Material* Mat = nullptr;
    MeshGeometry* Geo = nullptr;
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
  };

  PortalsApp(HINSTANCE hInstance);
  ~PortalsApp() override;

  PortalsApp(const PortalsApp&) = delete;
  PortalsApp(PortalsApp&&) = delete;
  PortalsApp& operator=(const PortalsApp&) = delete;
  PortalsApp& operator=(PortalsApp&&) = delete;

  bool Initialize() override;
  
protected:
  void OnResize() override;
  void Update(const GameTimer& gt) override;
  void Draw(const GameTimer& gt) override;
  
  void OnMouseDown(WPARAM btnState, int x, int y) override;
  void OnMouseUp(WPARAM btnState, int x, int y) override;
  void OnMouseMove(WPARAM btnState, int x, int y) override;

private:
  void LoadTexture(const std::string& name, const std::wstring& path);
  void BuildRootSignature();
  void BuildDescriptorHeaps();
  void BuildShadersAndInputLayout();
  void BuildShapeGeometry();
  void BuildPSOs();
  void BuildMaterials();

  void ReadRoomFile(const std::string& path);
  
  void UpdateCameraAndPortals(float dt, bool modifyPortal);

  DirectionalLight mDirLights[3];

  // Mouse
  POINT mLastMousePos;
  bool mRightButtonIsDown;

  // Camera
  Camera mLeftCamera;
  Camera mRightCamera;
  Camera* mCurrentCamera;
  XMMATRIX mLeftViewProj;
  float mLeftViewScale;
  XMMATRIX mRightViewProj;
  float mRightViewScale;

  // Room
  Room mRoom;
  /*PhongMaterial mWallsMaterial;
  XMMATRIX mWallsTexTransform;
  PhongMaterial mFloorMaterial;
  XMMATRIX mFloorTexTransform;
  PhongMaterial mCeilingMaterial;
  XMMATRIX mCeilingTexTransform;*/

  // Portal
  Portal mOrangePortal;
  Portal mBluePortal;
  Portal* mCurrentPortal;
  Portal* mOtherPortal;
  bool mPlayerIntersectOrangePortal;
  bool mPlayerIntersectBluePortal;

  // Player
  FirstPersonObject mPlayer;
  //PhongMaterial mPlayerMaterial;
  //XMMATRIX mPlayerTexTransform;

  // D3D12 stuff
  std::vector<FrameResource> mFrameResources;
  FrameResource* mCurrentFrameResource;
  int mCurrentFrameResourceIndex;

  UINT mCbvSrvDescriptorSize;

  ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

  ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

  std::unordered_map<std::string, MeshGeometry> mGeometries;
  std::unordered_map<std::string, PhongMaterial> mMaterials;
  std::unordered_map<std::string, Texture> mTextures;
  std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
  std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

  std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

  ComPtr<ID3D12PipelineState> mPSO = nullptr;

  std::vector<RenderItem> mRenderItems;

  PassConstants mMainPassCB;
};
