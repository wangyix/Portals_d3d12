#pragma once

#include "d3dApp.h" // Include this first

#include "Camera.h"
#include "Light.h"
#include "Room.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

using namespace DirectX;


class Portal;


class PortalsApp : public D3DApp {
public:
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
  PhongMaterial mWallsMaterial;
  XMMATRIX mWallsTexTransform;
  PhongMaterial mFloorMaterial;
  XMMATRIX mFloorTexTransform;
  PhongMaterial mCeilingMaterial;
  XMMATRIX mCeilingTexTransform;

  // Portal
  Portal mOrangePortal;
  Portal mBluePortal;
  Portal* mCurrentPortal;
  Portal* mOtherPortal;
  bool mPlayerIntersectOrangePortal;
  bool mPlayerIntersectBluePortal;

  // Player
  FirstPersonObject mPlayer;
  PhongMaterial mPlayerMaterial;
  XMMATRIX mPlayerTexTransform;
};
