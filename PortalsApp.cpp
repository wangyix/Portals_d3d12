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

  std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers()
  {
    // Applications usually only need a handful of samplers.  So just define them all up front
    // and keep them available as part of the root signature.  

    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
      0, // shaderRegister
      D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
      D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
      D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
      D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
      1, // shaderRegister
      D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
      D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
      D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
      D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
      2, // shaderRegister
      D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
      D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
      D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
      D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
      3, // shaderRegister
      D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
      D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
      D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
      D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
      4, // shaderRegister
      D3D12_FILTER_ANISOTROPIC, // filter
      D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
      D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
      D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
      0.0f,                             // mipLODBias
      8);                               // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
      5, // shaderRegister
      D3D12_FILTER_ANISOTROPIC, // filter
      D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
      D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
      D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
      0.0f,                              // mipLODBias
      8);                                // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC shadow(
      6, // shaderRegister
      D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
      D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
      D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
      D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
      0.0f,                               // mipLODBias
      16,                                 // maxAnisotropy
      D3D12_COMPARISON_FUNC_LESS_EQUAL,
      D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

    return {
      pointWrap, pointClamp,
      linearWrap, linearClamp,
      anisotropicWrap, anisotropicClamp,
      shadow
    };
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
  mClientWidth = 1280;
  mClientHeight = 720;

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

  mLeftCamera.SetLens(0.01f, 500.0f, 0.25 * PI);
  mRightCamera.SetLens(0.01f, 500.0f, 0.25 * PI);
  mRightCamera.AttachToObject(&mPlayer);
}

PortalsApp::~PortalsApp() {
  if (md3dDevice != nullptr)
    FlushCommandQueue();
}

bool PortalsApp::Initialize() {
  if (!D3DApp::Initialize())
    return false;
  
  // Reset the command list to prep for initialization commands.
  ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

  LoadTexture("orange_portal", L"textures/orange_portal2.dds");
  LoadTexture("blue_portal", L"textures/blue_portal2.dds");
  LoadTexture("wall", L"textures/floor.dds");
  LoadTexture("player", L"textures/stone.dds");

  BuildRootSignature();
  BuildDescriptorHeaps();
  BuildShadersAndInputLayout();
  
  
  ReadRoomFile("room.txt");

  return true;
}

void PortalsApp::LoadTexture(const std::string& name, const std::wstring& path) {
  Texture& texture = mTextures[name];
  texture.Name = name;
  texture.Filename = path;
  ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
    mCommandList.Get(), path.c_str(), texture.Resource, texture.UploadHeap));
}

void PortalsApp::BuildRootSignature() {
  // Portal textures
  CD3DX12_DESCRIPTOR_RANGE texTable0;
  texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);

  // Material textures
  CD3DX12_DESCRIPTOR_RANGE texTable1;
  texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 2, 0);

  CD3DX12_ROOT_PARAMETER rootParameters[5];
  // Order from most frequent to least frequent.
  rootParameters[0].InitAsConstantBufferView(0);      // cbPerObject
  rootParameters[1].InitAsConstantBufferView(1);      // cbPass
  rootParameters[2].InitAsShaderResourceView(0, 1);   // gMaterialData
  // gPortalADiffuseMap, gPortalBDiffuseMap
  rootParameters[3].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
  // gTextureMaps
  rootParameters[4].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

  const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
      0,                                  // shaderRegister
      D3D12_FILTER_ANISOTROPIC,           // filter
      D3D12_TEXTURE_ADDRESS_MODE_WRAP,    // addressU
      D3D12_TEXTURE_ADDRESS_MODE_WRAP,    // addressV
      D3D12_TEXTURE_ADDRESS_MODE_WRAP,    // addressW
      0.0f,                               // mipLODBias
      8);                                 // maxAnisotropy
  const CD3DX12_STATIC_SAMPLER_DESC anisotropicBlackBorder(
      1,                                            // shaderRegister
      D3D12_FILTER_ANISOTROPIC,                     // filter
      D3D12_TEXTURE_ADDRESS_MODE_BORDER,            // addressU
      D3D12_TEXTURE_ADDRESS_MODE_BORDER,            // addressV
      D3D12_TEXTURE_ADDRESS_MODE_BORDER,            // addressW
      0.0f,                                         // mipLODBias
      8,                                            // maxAnisotropy
      D3D12_COMPARISON_FUNC_LESS_EQUAL,             // comparisonFunc
      D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK); // borderColor
  CD3DX12_STATIC_SAMPLER_DESC staticSamplers[2] = { anisotropicWrap, anisotropicBlackBorder };

  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
      _countof(rootParameters), rootParameters, _countof(staticSamplers), staticSamplers,
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  
  // Create root signature
  ComPtr<ID3DBlob> serializedRootSig = nullptr;
  ComPtr<ID3DBlob> errorBlob = nullptr;
  HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
    serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
  if (errorBlob != nullptr) {
    ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
  }
  ThrowIfFailed(hr);
  ThrowIfFailed(md3dDevice->CreateRootSignature(
      0,
      serializedRootSig->GetBufferPointer(),
      serializedRootSig->GetBufferSize(),
      IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void PortalsApp::BuildDescriptorHeaps() {
  // Create the SRV heap.
  D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
  srvHeapDesc.NumDescriptors = 4;
  srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

  CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(
      mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

  ComPtr<ID3D12Resource> tex2DList[4] = {
    mTextures["orange_portal"].Resource,
    mTextures["blue_portal"].Resource,
    mTextures["wall"].Resource,
    mTextures["player"].Resource
  };

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Texture2D.MostDetailedMip = 0;
  srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
  for (int i = 0; i < _countof(tex2DList); ++i) {
    srvDesc.Format = tex2DList[i]->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = tex2DList[i]->GetDesc().MipLevels;
    md3dDevice->CreateShaderResourceView(tex2DList[i].Get(), &srvDesc, hDescriptor);
    // Next descriptor
    hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
  }
}

void PortalsApp::BuildShadersAndInputLayout() {
  std::string numLightsStr = std::to_string(_countof(mDirLights));
  D3D_SHADER_MACRO defines[] = {
      { "NUM_LIGHTS", numLightsStr.c_str() },
      { nullptr, nullptr },
      { nullptr, nullptr },
      { nullptr, nullptr },
      { nullptr, nullptr }};
  mShaders["PortalBoxVS"] = d3dUtil::CompileShader(L"fx/PortalBox.hlsl", defines, "VS", "vs_5_1");
  mShaders["PortalBoxPS"] = d3dUtil::CompileShader(L"fx/PortalBox.hlsl", defines, "PS", "ps_5_1");

  defines[1] = { "DRAW_PORTAL_A", nullptr };
  defines[2] = { "DRAW_PORTAL_B", nullptr };
  mShaders["DefaultVS"] = d3dUtil::CompileShader(L"fx/Default.hlsl", defines, "VS", "vs_5_1");
  mShaders["DefaultPS"] = d3dUtil::CompileShader(L"fx/Default.hlsl", defines, "PS", "ps_5_1");
  defines[3] = { "CLIP_PLANE", nullptr };
  mShaders["DefaultClipPS"] = d3dUtil::CompileShader(L"fx/Default.hlsl", defines, "PS", "ps_5_1");

  mInputLayout = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
  };
}



void PortalsApp::OnResize() {
  D3DApp::OnResize();
}

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
  std::ifstream ifs(path, std::ifstream::in);
  if (!ifs.good()) {
    throw std::exception("Could not open room file.");
  }

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
  } catch (DxException& e) {
    MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
    return 0;
  } catch (std::exception& e) {
    std::string error(e.what());
    std::wstring ws;
    std::copy(error.begin(), error.end(), std::back_inserter(ws));
    MessageBox(nullptr, ws.c_str(), L"Error", MB_OK);
    return 0;
  }
}