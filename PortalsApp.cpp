#include "PortalsApp.h"

#include "SpherePath.h"

namespace {
  
  bool GetNextDataLine(std::ifstream* ifs, std::string* line) {
    line->clear();
    do {
      *ifs >> std::noskipws;
    } while (std::getline(*ifs, *line) && !line->empty() && line->front() != 'w');
    return !ifs->eof();
  }

} // namespace

PortalsApp::PortalsApp(HINSTANCE hInstance)
  : D3DApp(hInstance),
    mRightButtonIsDown(false),
    mCurrentCamera(&mLeftCamera),
    mCurrentPortal(&mOrangePortal),
    mOtherPortal(&mBluePortal),
    mPlayerIntersectOrangePortal(false),
    mPlayerIntersectBluePortal(false) {
  // 3 directional lights
  mDirLights[0].Ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
  mDirLights[0].Diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
  mDirLights[0].Specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
  mDirLights[0].Direction = XMFLOAT3(0.57735f, -0.57735f, 0.57735f);

  mDirLights[1].Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
  mDirLights[1].Diffuse = XMFLOAT4(0.20f, 0.20f, 0.20f, 1.0f);
  mDirLights[1].Specular = XMFLOAT4(0.25f, 0.25f, 0.25f, 1.0f);
  mDirLights[1].Direction = XMFLOAT3(-0.57735f, -0.57735f, 0.57735f);

  mDirLights[2].Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
  mDirLights[2].Diffuse = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
  mDirLights[2].Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
  mDirLights[2].Direction = XMFLOAT3(0.0f, -0.707f, -0.707f);

  // room values
  mWallsMaterial.Ambient = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
  mWallsMaterial.Diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
  mWallsMaterial.Specular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
  mWallsMaterial.Reflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
  mWallsTexTransform = XMMatrixScaling(0.25f, 0.25f, 1.0f);

  mFloorMaterial.Ambient = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
  mFloorMaterial.Diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
  mFloorMaterial.Specular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
  mFloorMaterial.Reflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
  mFloorTexTransform = XMMatrixScaling(0.25f, 0.25f, 1.0f);

  mCeilingMaterial.Ambient = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
  mCeilingMaterial.Diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
  mCeilingMaterial.Specular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
  mCeilingMaterial.Reflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
  mCeilingTexTransform = XMMatrixScaling(0.25f, 0.25f, 1.0f);

  // player values
  mPlayerMaterial.Ambient = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
  mPlayerMaterial.Diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
  mPlayerMaterial.Specular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
  mPlayerMaterial.Reflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
  mPlayerTexTransform = XMMatrixIdentity();
}

PortalsApp::~PortalsApp() {

}

bool PortalsApp::Initialize() {
  if (!D3DApp::Initialize())
    return false;
  
  ReadRoomFile(ROOM_FILE_PATH);

  mLeftCamera.SetLens(0.01f, 500.0f, 0.25 * PI);
  mRightCamera.SetLens(0.01f, 500.0f, 0.25 * PI);
  mRightCamera.AttachToObject(&mPlayer);

  // Create shader resources from textures

  // Build geometry buffers for room, portal, player

  return true;
}

void PortalsApp::OnResize() {}

void PortalsApp::Update(const GameTimer& gt) {
  // Portals cannot be changed if either portal intersects the player or the spectator camera
  bool modifyPortal = (!mPlayerIntersectOrangePortal && !mPlayerIntersectBluePortal) &&
    !mOrangePortal.DiscIntersectSphere(
        mLeftCamera.GetPosition(), mLeftCamera.GetBoundingSphereRadius() + 0.001f) &&
    !mBluePortal.DiscIntersectSphere(
        mLeftCamera.GetPosition(), mLeftCamera.GetBoundingSphereRadius() + 0.001f);

  UpdateCameraAndPortals(gt.DeltaTime(), modifyPortal);
}

void PortalsApp::Draw(const GameTimer& gt) {}

void PortalsApp::OnMouseDown(WPARAM btnState, int x, int y) {
  mLastMousePos.x = x;
  mLastMousePos.y = y;

  if ((btnState & MK_RBUTTON) != 0)
    mRightButtonIsDown = true;

  SetCapture(mhMainWnd);
}

void PortalsApp::OnMouseUp(WPARAM btnState, int x, int y) {
  if ((btnState & MK_RBUTTON) == 0)
    mRightButtonIsDown = false;

  ReleaseCapture();
}

void PortalsApp::OnMouseMove(WPARAM btnState, int x, int y) {
  if ((btnState & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)) != 0) {
    // Make each pixel correspond to a quarter of a degree.
    float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
    float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));
    mCurrentCamera->RotateRight(dx);
    mCurrentCamera->RotateUp(-dy);
  }
  mLastMousePos.x = x;
  mLastMousePos.y = y;
}




void PortalsApp::ReadRoomFile(const std::string& path) {
  std::ifstream ifs(path);
  std::string line;
  float a, b, c, d, e, f;

  // Left camera initial position
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f %f %f", &a, &b, &c);
  mLeftCamera.SetPosition(XMFLOAT3(a, b, c));

  // Player
  // Radius
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f", &a);
  mPlayer.SetBoundingSphereRadius(a);
  // Initial position
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f %f %f", &a, &b, &c);
  mPlayer.SetPosition(XMFLOAT3(a, b, c));

  // Orange portal
  // Initial radius
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f", &a);
  mOrangePortal.SetIntendedPhysicalRadius(a);
  // Initial position
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f %f %f", &a, &b, &c);
  mOrangePortal.SetPosition(XMFLOAT3(a, b, c));
  // Initial normal
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f %f %f", &a, &b, &c);
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f %f %f", &d, &e, &f);
  mOrangePortal.SetNormalAndUp(XMFLOAT3(a, b, c), XMFLOAT3(d, e, f));

  // Blue portal
  // Initial radius
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f", &a);
  mBluePortal.SetIntendedPhysicalRadius(a);
  // Initial position
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f %f %f", &a, &b, &c);
  mBluePortal.SetPosition(XMFLOAT3(a, b, c));
  // Initial normal
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f %f %f", &a, &b, &c);
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f %f %f", &d, &e, &f);
  mBluePortal.SetNormalAndUp(XMFLOAT3(a, b, c), XMFLOAT3(d, e, f));

  // Floor, ceiling heights
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f %f", &a, &b);
  mRoom.SetFloorAndCeiling(a, b);

  // Room boundary polygons
  std::vector<std::vector<XMFLOAT2>> polygonList;
  while (GetNextDataLine(&ifs, &line)) {
    int num_vertices;
    sscanf_s(line.c_str(), "%d", &num_vertices);
    std::vector<XMFLOAT2> vertices(num_vertices);
    for (int i = 0; i < num_vertices; ++i) {
      GetNextDataLine(&ifs, &line);
      sscanf_s(line.c_str(), "%f %f", &a, &b);
      vertices[i] = XMFLOAT2(a, b);
    }
    polygonList.push_back(std::move(vertices));
  }
  mRoom.SetTopography(polygonList);
  mRoom.PrintBoundaries();

  ifs.close();
}

void PortalsApp::UpdateCameraAndPortals(float dt, bool modifyPortal) {
  // Select which portal to control
  if (GetAsyncKeyState('O') & 0x8000) {
    mCurrentPortal = &mOrangePortal;
    mOtherPortal = &mBluePortal;
  } else if (GetAsyncKeyState('B') & 0x8000) {
    mCurrentPortal = &mBluePortal;
    mOtherPortal = &mOrangePortal;
  }

  // Select which camera to control
  if (GetAsyncKeyState('1') & 0x8000)
    mCurrentCamera = &mLeftCamera;
  else if (GetAsyncKeyState('2') & 0x8000)
    mCurrentCamera = &mRightCamera;
  
  // Update camera position
  float forwardSteps = 0.0f;
  float rightSteps = 0.0f;
  float upSteps = 0.f;
  if (GetAsyncKeyState('W') & 0x8000)
    forwardSteps += 1.0f;
  if (GetAsyncKeyState('S') & 0x8000)
    forwardSteps -= 1.0f;
  if (GetAsyncKeyState('A') & 0x8000)
    rightSteps -= 1.0f;
  if (GetAsyncKeyState('D') & 0x8000)
    rightSteps += 1.0f;
  if (GetAsyncKeyState(VK_SPACE) & 0x8000)
    upSteps += 1.0f;
  if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
    upSteps -= 1.0f;
  XMFLOAT3 dir = forwardSteps * mCurrentCamera->GetLook() +
    rightSteps * mCurrentCamera->GetRight() + upSteps * mCurrentCamera->GetBodyUp();
  float dir_length = XMFloat3Length(dir);
  if (dir_length != 0.0f) {
    float speed = CAMERA_MOVEMENT_SPEED;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
      speed *= CAMERA_MOVEMENT_SPRINT_MULTIPLIER;
    dir = dir / dir_length;
    SpherePath::MoveCameraAlongPathIterative(*mCurrentCamera, dir, speed * dt,
      mRoom, mOrangePortal, mBluePortal);
  }
  
  // Update camera orientation
  if (GetAsyncKeyState('L') & 0x8000) {
    // Reset
    mCurrentCamera->Level();
  } else {
    // Roll
    float rightRollUnits = 0.0f;
    if (GetAsyncKeyState('Q') & 0x8000)
      rightRollUnits -= 1.0f;
    if (GetAsyncKeyState('E') & 0x8000)
      rightRollUnits += 1.0f;
    if (rightRollUnits != 0.0f)
      mCurrentCamera->RollRight(rightRollUnits * CAMERA_ROLL_SPEED / 180.0f * PI * dt);
  }

  // Update current portal
  if (modifyPortal) {
    // Update portal position
    if (mRightButtonIsDown) {
      mRoom.PortalRelocate(mCurrentCamera->GetPosition(), mCurrentCamera->GetLook(),
          mCurrentPortal, *mOtherPortal);
    }
    // Update portal orientation
    float PortalLeftRotateUnits = 0.0f;
    if (GetAsyncKeyState(VK_LEFT) & 0x8000)
      PortalLeftRotateUnits += 1.0f;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
      PortalLeftRotateUnits -= 1.0f;
    if (PortalLeftRotateUnits != 0.0f) {
      mCurrentPortal->RotateLeftAroundNormal(
          PortalLeftRotateUnits * PORTAL_ROTATE_SPEED / 180.0f * PI * dt);
      mCurrentPortal->Orthonormalize();
    }
    // Update portal size
    float PortalSizeIncreaseUnits = 0;
    if (GetAsyncKeyState(VK_UP) & 0x8000)
      PortalSizeIncreaseUnits += 1.0f;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000)
      PortalSizeIncreaseUnits -= 1.0f;
    if (PortalSizeIncreaseUnits != 0.0f)
    {
      float NewRadius = mCurrentPortal->GetPhysicalRadius() +
          (PortalSizeIncreaseUnits * PORTAL_SIZE_CHANGE_SPEED * dt);
      mCurrentPortal->SetIntendedPhysicalRadius(NewRadius);
    }
  }
}

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