#pragma once

#include "d3dApp.h"

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

};