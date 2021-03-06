#pragma once

#include "d3dApp.h" // Include this first

#include "Camera.h"
#include "FrameResource.h"
#include "Light.h"
#include "Room.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

const int gNumFrameResources = 3;

class PortalsApp : public D3DApp {
public:
  struct RenderItem {
    XMMATRIX World = XMMatrixIdentity();
    XMMATRIX TexTransform = XMMatrixIdentity();

    // Dirty flag indicating the object data has changed and we need to update the constant buffer.
    // Because we have an object cbuffer for each FrameResource, we have to apply the
    // update to each FrameResource.  Thus, when we modify obect data we should set 
    // NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
    int NumFramesDirty = gNumFrameResources;

    // Index into GPU constant buffer corresponding to the ObjectCB for this render item.
    UINT ObjCBIndex = -1;

    PhongMaterial* Mat = nullptr;
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
  void Update(float dt) override;
  void Draw(float dt) override;
  
  void OnMouseDown(WPARAM btnState, int x, int y) override;
  void OnMouseUp(WPARAM btnState, int x, int y) override;
  void OnMouseMove(WPARAM btnState, int x, int y) override;

private:
  void LoadTexture(const std::string& name, const std::wstring& path);
  void BuildRootSignature();
  void BuildDescriptorHeaps();
  void BuildShadersAndInputLayout();
  void BuildShapeGeometry();
  void BuildMaterials();
  void BuildRenderItems();
  void BuildFrameResources();
  void BuildPSOs();

  void ReadRoomFile(const std::string& path);
  
  void OnKeyboardInput(float dt, bool modifyPortal);
  void UpdateMaterialBuffer();
  void UpdateObjectCBs();
  void UpdateClipPlaneCB(
      int index, const XMFLOAT3& position, const XMFLOAT3& normal, float offset,
      const XMFLOAT3& position2, const XMFLOAT3& normal2, float offset2);
  void UpdateWorld2CB(int index, const XMMATRIX& world2);
  void UpdatePassCB(
      int index, const XMMATRIX& viewProj, const XMFLOAT3& eyePosW, float distDilation);
  void UpdateFrameCB();

  void DrawRenderItem(
    ID3D12GraphicsCommandList* cmdList, RenderItem* ri, bool sameAsPrevious = false);

  void DrawIntersectingPlayerRealHalves(
    int clipPlanePortalCBIndex, int clipPlaneOtherPortalCBIndex, int world2ThisToOtherCBIndex);
  
  void DrawRoomAndPlayerIterations(
      UINT stencilRef, RenderItem* portalBoxRi, int CBIndexBase, int numIterations,
      int clipPlaneCBIndex, bool drawPlayers);

  void DrawPlayerIterations(
    UINT stencilRef, int CBIndexBase, int numIterations, const std::string& psoName);

  void DrawRoomsAndIntersectingPlayersForPortal(
      UINT stencilRef, RenderItem* portalBoxRi, int CBIndexBase, int numIterations,
      int clipPlanePortalCBIndex, int clipPlaneOtherPortalCBIndex, bool playerIntersectPortal,
      int world2ThisToOtherCBIndex, int world2OtherToThisCBIndex);

  void DrawPortalBoxToCoverDepthHoleAndZeroStencil(UINT stencilRef, RenderItem* portalBoxRi);

  XMFLOAT3 mAmbientLight;
  DirectionalLight mDirLights[NUM_LIGHTS];

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

  // Portal
  Portal mPortalA;
  Portal mPortalB;
  Portal* mCurrentPortal;   // Portal currently selected by user
  Portal* mOtherPortal;
  bool mPlayerIntersectPortalA;
  bool mPlayerIntersectPortalB;
  XMMATRIX mPortalAToB;
  XMMATRIX mPortalBToA;

  // Player
  FirstPersonObject mPlayer;

  // D3D12 stuff
  std::vector<std::unique_ptr<FrameResource>> mFrameResources;
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

  RenderItem mRoomRenderItem;
  RenderItem mPlayerRenderItem;
  RenderItem mPortalBoxARenderItem;
  RenderItem mPortalBoxBRenderItem;
  RenderItem* mCurrentPortalBoxRenderItem;

  PassConstants mMainPassCB;
};
