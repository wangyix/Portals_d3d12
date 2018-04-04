#include "PortalsApp.h"

PortalsApp::PortalsApp(HINSTANCE hInstance)
  : D3DApp(hInstance), mCurrentCamera(&mLeftCamera) {
  
}

PortalsApp::~PortalsApp() {

}

bool PortalsApp::Initialize() {
  return true;
}

void PortalsApp::OnResize() {}

void PortalsApp::Update(const GameTimer& gt) {}

void PortalsApp::Draw(const GameTimer& gt) {}

void PortalsApp::OnMouseDown(WPARAM btnState, int x, int y) {};
void PortalsApp::OnMouseUp(WPARAM btnState, int x, int y) {};
void PortalsApp::OnMouseMove(WPARAM btnState, int x, int y) {};


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
  PSTR cmdLine, int showCmd)
{
  // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  try
  {
    PortalsApp theApp(hInstance);
    if (!theApp.Initialize())
      return 0;

    return theApp.Run();
  } catch (DxException& e)
  {
    MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
    return 0;
  }
}