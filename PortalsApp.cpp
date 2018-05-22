#include "PortalsApp.h"

#include "GeometryGenerator.h"
#include "SpherePath.h"

namespace {
  const UINT CB_PER_OBJECT_ROOT_INDEX = 0;
  const UINT CB_CLIP_PLANE_ROOT_INDEX = 1;
  const UINT CB_WORLD2_ROOT_INDEX = 2;
  const UINT CB_PER_PASS_ROOT_INDEX = 3;
  const UINT CB_PER_FRAME_ROOT_INDEX = 4;
  const UINT SRV_MATERIAL_DATA_ROOT_INDEX = 5;
  const UINT DT_PORTAL_MAPS_ROOT_INDEX = 6;
  const UINT DT_TEXTURE_MAPS_ROOT_INDEX = 7;
  const UINT NUM_ROOT_PARAMETERS = 8;

  const int CLIP_PLANE_DUMMY_CB_INDEX = 0;
  const int CLIP_PLANE_PORTAL_A_B_CB_INDEX = 1;
  const int CLIP_PLANE_PORTAL_B_A_CB_INDEX = 2;
  const int NUM_CLIP_PLANE_CBS = 3;

  const int WORLD2_IDENTITY_CB_INDEX = 0;
  const int WORLD2_PORTAL_A_TO_B_CB_INDEX = 1;
  const int WORLD2_PORTAL_B_TO_A_CB_INDEX = 2;
  const int NUM_WORLD2_CBS = 3;

  bool GetNextDataLine(std::ifstream* ifs, std::string* line) {
    line->clear();
    do {
      *ifs >> std::ws;  // read leading whitespace
      // line can't be empty, so line->front() is safe
    } while (std::getline(*ifs, *line) && line->front() == '#');
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
    mCurrentPortal(&mPortalA),
    mOtherPortal(&mPortalB),
    mPlayerIntersectPortalA(false),
    mPlayerIntersectPortalB(false),
    mCurrentPortalBoxRenderItem(&mPortalBoxARenderItem) {
  mClientWidth = 1280;
  mClientHeight = 720;
  mMinFrameTime = 1.0f / 300.0f;

  mAmbientLight = XMFLOAT3(0.3f, 0.3f, 0.3f);
  mDirLights[0].Strength = XMFLOAT3(0.7f, 0.7f, 0.7f);
  mDirLights[0].Direction = XMFLOAT3(0.57735f, -0.57735f, 0.57735f);
  mDirLights[1].Strength = XMFLOAT3(0.5f, 0.5f, 0.5f);
  mDirLights[1].Direction = XMFLOAT3(-0.57735f, -0.57735f, 0.57735f);
  mDirLights[2].Strength = XMFLOAT3(0.5f, 0.5f, 0.5f);
  mDirLights[2].Direction = XMFLOAT3(0.0f, -0.707f, -0.707f);

  mLeftCamera.SetLens(0.01f, 500.0f, 0.25 * PI);
  mRightCamera.SetLens(0.01f, 500.0f, 0.25 * PI);
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

  // Get the increment size of a descriptor in this heap type.  This is hardware specific, 
  // so we have to query this information.
  mCbvSrvDescriptorSize =
      md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  
  ReadRoomFile("room.txt");
  mRightCamera.AttachToObject(&mPlayer);  // Updates mRightCamera's position, orientation
  mPortalAToB = Portal::CalculateVirtualizationMatrix(mPortalA, mPortalB);
  mPortalBToA = Portal::CalculateVirtualizationMatrix(mPortalB, mPortalA);

  LoadTexture("portalA", L"textures/orange_portal2.dds");
  LoadTexture("portalB", L"textures/blue_portal2.dds");
  LoadTexture("room", L"textures/tile.dds");
  LoadTexture("player", L"textures/stone.dds");

  mPortalA.SetTextureRadiusRatio(PORTAL_TEX_RAD_RATIO);
  mPortalB.SetTextureRadiusRatio(PORTAL_TEX_RAD_RATIO);

  BuildRootSignature();
  BuildDescriptorHeaps();
  BuildShadersAndInputLayout();
  BuildShapeGeometry();
  BuildMaterials();
  BuildRenderItems();
  BuildFrameResources();
  BuildPSOs();
  
  // Execute the initialization commands.
  ThrowIfFailed(mCommandList->Close());
  ID3D12CommandList* cmdList = mCommandList.Get();
  mCommandQueue->ExecuteCommandLists(1, &cmdList);

  // Wait until initialization is complete.
  FlushCommandQueue();

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

  CD3DX12_ROOT_PARAMETER rootParameters[NUM_ROOT_PARAMETERS];
  // Order from most frequent to least frequent.
  rootParameters[CB_PER_OBJECT_ROOT_INDEX].InitAsConstantBufferView(0);   // cbPerObject
  rootParameters[CB_CLIP_PLANE_ROOT_INDEX].InitAsConstantBufferView(1);   // cbClipPlane
  rootParameters[CB_WORLD2_ROOT_INDEX].InitAsConstantBufferView(2);  // cbWorld2
  rootParameters[CB_PER_PASS_ROOT_INDEX].InitAsConstantBufferView(3);     // cbPass
  rootParameters[CB_PER_FRAME_ROOT_INDEX].InitAsConstantBufferView(4);    // cbFrame
  rootParameters[SRV_MATERIAL_DATA_ROOT_INDEX].InitAsShaderResourceView(0, 1);   // gMaterialData
  // gPortalADiffuseMap, gPortalBDiffuseMap
  rootParameters[DT_PORTAL_MAPS_ROOT_INDEX].InitAsDescriptorTable(
      1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
  // gTextureMaps
  rootParameters[DT_TEXTURE_MAPS_ROOT_INDEX].InitAsDescriptorTable(
      1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

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

  // Fill the heap with descriptors.
  ComPtr<ID3D12Resource> tex2DList[4] = {
    // Put the descriptors for the textures used for gTextureMaps[2] first in the heap so that
    // PhongMaterial::DiffuseSrvHeapIndex matches PhongMaterialData::DiffuseMapIndex.
    mTextures["room"].Resource,
    mTextures["player"].Resource,
    mTextures["portalA"].Resource,
    mTextures["portalB"].Resource
  };
  CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(
      mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
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
  D3D_SHADER_MACRO defines[] = {
      { nullptr, nullptr },
      { nullptr, nullptr },
      { nullptr, nullptr },
      { nullptr, nullptr },
      { nullptr, nullptr },
      { nullptr, nullptr } };
  mShaders["portalBoxVS"] = d3dUtil::CompileShader(L"fx/PortalBox.hlsl", defines, "VS", "vs_5_1");
  mShaders["portalBoxPS"] = d3dUtil::CompileShader(L"fx/PortalBox.hlsl", defines, "PS", "ps_5_1");
  defines[0] = { "CLEAR_DEPTH", nullptr };
  mShaders["portalBoxClearDepthVS"] = d3dUtil::CompileShader(L"fx/PortalBox.hlsl", defines, "VS", "vs_5_1");  
  defines[0] = { "CLIP_PLANE", nullptr };
  mShaders["portalBoxClipPS"] = d3dUtil::CompileShader(L"fx/PortalBox.hlsl", defines, "PS", "ps_5_1");

  std::string numLightsStr = std::to_string(NUM_LIGHTS);
  std::string portalTexRadRatioStr = std::to_string(PORTAL_TEX_RAD_RATIO);
  defines[0] = { "NUM_LIGHTS", numLightsStr.c_str() };
  defines[1] = { "PORTAL_TEX_RAD_RATIO", portalTexRadRatioStr.c_str() };
  mShaders["defaultVS"] = d3dUtil::CompileShader(L"fx/Default.hlsl", defines, "VS", "vs_5_1");
  defines[2] = { "CLIP_PLANE", nullptr };
  mShaders["defaultClipPS"] = d3dUtil::CompileShader(L"fx/Default.hlsl", defines, "PS", "ps_5_1");
  defines[3] = { "CLIP_PLANE_2", nullptr };
  mShaders["defaultClipTwicePS"] = d3dUtil::CompileShader(L"fx/Default.hlsl", defines, "PS", "ps_5_1");
  defines[3] = { "DRAW_PORTALS", nullptr };
  mShaders["defaultPortalsVS"] = d3dUtil::CompileShader(L"fx/Default.hlsl", defines, "VS", "vs_5_1");
  mShaders["defaultPortalsClipPS"] = d3dUtil::CompileShader(L"fx/Default.hlsl", defines, "PS", "ps_5_1");

  mInputLayout = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
  };
}

void PortalsApp::BuildShapeGeometry() {
  UINT numTotalIndices = 0;
  INT numTotalVertices = 0;

  // Generate room mesh and wall, floor, ceiling submeshes.
  SubmeshGeometry wallsSubmesh;
  SubmeshGeometry floorSubmesh;
  SubmeshGeometry ceilingSubmesh;
  GeometryGenerator::MeshData roomMesh;
  mRoom.BuildMeshData(&roomMesh, &wallsSubmesh, &floorSubmesh, &ceilingSubmesh);
  SubmeshGeometry roomSubmesh;
  roomSubmesh.IndexCount = static_cast<UINT>(roomMesh.Indices.size());
  roomSubmesh.StartIndexLocation = numTotalIndices;
  roomSubmesh.BaseVertexLocation = numTotalVertices;
  numTotalIndices += static_cast<UINT>(roomMesh.Indices.size());
  numTotalVertices += static_cast<INT>(roomMesh.Vertices.size());

  // Generate player mesh and submesh.
  GeometryGenerator::MeshData playerMesh;
  GeometryGenerator::GenerateSphere(playerMesh, 1.0f, 3);
  SubmeshGeometry playerSubmesh;
  playerSubmesh.IndexCount = static_cast<UINT>(playerMesh.Indices.size());
  playerSubmesh.StartIndexLocation = numTotalIndices;
  playerSubmesh.BaseVertexLocation = numTotalVertices;
  numTotalIndices += static_cast<UINT>(playerMesh.Indices.size());
  numTotalVertices += static_cast<INT>(playerMesh.Vertices.size());

  // Generate portal-box mesh and submesh
  GeometryGenerator::MeshData portalBoxMesh;
  Portal::BuildBoxMeshData(&portalBoxMesh);
  SubmeshGeometry portalBoxSubmesh;
  portalBoxSubmesh.IndexCount = static_cast<UINT>(portalBoxMesh.Indices.size());
  portalBoxSubmesh.StartIndexLocation = numTotalIndices;
  portalBoxSubmesh.BaseVertexLocation = numTotalVertices;
  numTotalIndices += static_cast<UINT>(portalBoxMesh.Indices.size());
  numTotalVertices += static_cast<INT>(portalBoxMesh.Vertices.size());

  // Concatenate room and player mesh vertices into one vector.
  std::vector<Vertex> vertices(numTotalVertices);
  int k = 0;
  for (size_t i = 0; i < roomMesh.Vertices.size(); ++i, ++k) {
    vertices[k].Pos = roomMesh.Vertices[i].Position;
    vertices[k].Normal = roomMesh.Vertices[i].Normal;
    vertices[k].TexC = roomMesh.Vertices[i].TexCoord;
  }
  for (size_t i = 0; i < playerMesh.Vertices.size(); ++i, ++k) {
    vertices[k].Pos = playerMesh.Vertices[i].Position;
    vertices[k].Normal = playerMesh.Vertices[i].Normal;
    vertices[k].TexC = playerMesh.Vertices[i].TexCoord;
  }
  for (size_t i = 0; i < portalBoxMesh.Vertices.size(); ++i, ++k) {
    vertices[k].Pos = portalBoxMesh.Vertices[i].Position;
    vertices[k].Normal = portalBoxMesh.Vertices[i].Normal;
    vertices[k].TexC = portalBoxMesh.Vertices[i].TexCoord;
  }

  // Concatenate room and player mesh indices into one vector.
  std::vector<std::uint16_t> indices(numTotalIndices);
  k = 0;
  for (size_t i = 0; i < roomMesh.Indices.size(); ++i, ++k) {
    indices[k] = static_cast<std::uint16_t>(roomMesh.Indices[i]);
  }
  for (size_t i = 0; i < playerMesh.Indices.size(); ++i, ++k) {
    indices[k] = static_cast<std::uint16_t>(playerMesh.Indices[i]);
  }
  for (size_t i = 0; i < portalBoxMesh.Indices.size(); ++i, ++k) {
    indices[k] = static_cast<std::uint16_t>(portalBoxMesh.Indices[i]);
  }
  
  // Generate MeshGeometry of concatenated meshes.
  const UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(Vertex));
  const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint16_t));
  MeshGeometry* geo = &mGeometries["shapeGeo"];
  geo->Name = "shapeGeo";
  ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
  CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
  ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
  CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
  geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
    mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
  geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
    mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);
  geo->VertexByteStride = sizeof(Vertex);
  geo->VertexBufferByteSize = vbByteSize;
  geo->IndexFormat = DXGI_FORMAT_R16_UINT;
  geo->IndexBufferByteSize = ibByteSize;
  geo->DrawArgs["room"] = roomSubmesh;
  //geo->DrawArgs["walls"] = wallsSubmesh;
  //geo->DrawArgs["floor"] = floorSubmesh;
  //geo->DrawArgs["ceiling"] = ceilingSubmesh;
  geo->DrawArgs["player"] = playerSubmesh;
  geo->DrawArgs["portalBox"] = portalBoxSubmesh;
}

void PortalsApp::BuildMaterials() {
  PhongMaterial* roomMaterial = &mMaterials["room"];
  roomMaterial->Diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
  roomMaterial->Specular = XMFLOAT4(0.6f, 0.6f, 0.6f, 64.0f);
  roomMaterial->MatCBIndex = 0;
  roomMaterial->DiffuseSrvHeapIndex = 0;

  PhongMaterial* playerMaterial = &mMaterials["player"];
  playerMaterial->Diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
  playerMaterial->Specular = XMFLOAT4(0.6f, 0.6f, 0.6f, 64.0f);
  playerMaterial->MatCBIndex = 1;
  playerMaterial->DiffuseSrvHeapIndex = 1;
}

void PortalsApp::BuildRenderItems() {
  mRoomRenderItem.World = XMMatrixIdentity();
  mRoomRenderItem.TexTransform = XMMatrixScaling(0.25f, 0.25f, 1.0f);
  mRoomRenderItem.ObjCBIndex = 0;
  mRoomRenderItem.Mat = &mMaterials["room"];
  mRoomRenderItem.Geo = &mGeometries["shapeGeo"];
  mRoomRenderItem.PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
  const SubmeshGeometry& roomSubMesh = mRoomRenderItem.Geo->DrawArgs["room"];
  mRoomRenderItem.IndexCount = roomSubMesh.IndexCount;
  mRoomRenderItem.StartIndexLocation = roomSubMesh.StartIndexLocation;
  mRoomRenderItem.BaseVertexLocation = roomSubMesh.BaseVertexLocation;

  mPlayerRenderItem.World = mPlayer.GetWorldMatrix();   // Update whenever player moves
  mPlayerRenderItem.TexTransform = XMMatrixIdentity();
  mPlayerRenderItem.ObjCBIndex = 1;
  mPlayerRenderItem.Mat = &mMaterials["player"];
  mPlayerRenderItem.Geo = &mGeometries["shapeGeo"];
  mPlayerRenderItem.PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
  const SubmeshGeometry& playerSubmesh = mPlayerRenderItem.Geo->DrawArgs["player"];
  mPlayerRenderItem.IndexCount = playerSubmesh.IndexCount;
  mPlayerRenderItem.StartIndexLocation = playerSubmesh.StartIndexLocation;
  mPlayerRenderItem.BaseVertexLocation = playerSubmesh.BaseVertexLocation;

  mPortalBoxARenderItem.World = mPortalA.GetXYScaledPortalToWorldMatrix(); // Update whenever portal A moves
  mPortalBoxARenderItem.TexTransform = XMMatrixIdentity();    // unused
  mPortalBoxARenderItem.ObjCBIndex = 2;
  mPortalBoxARenderItem.Mat = &mMaterials["room"];            // unused
  mPortalBoxARenderItem.Geo = &mGeometries["shapeGeo"];
  mPortalBoxARenderItem.PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
  const SubmeshGeometry& portalBoxASubmesh = mPortalBoxARenderItem.Geo->DrawArgs["portalBox"];
  mPortalBoxARenderItem.IndexCount = portalBoxASubmesh.IndexCount;
  mPortalBoxARenderItem.StartIndexLocation = portalBoxASubmesh.StartIndexLocation;
  mPortalBoxARenderItem.BaseVertexLocation = portalBoxASubmesh.BaseVertexLocation;

  mPortalBoxBRenderItem.World = mPortalB.GetXYScaledPortalToWorldMatrix(); // Update whenever portal B moves
  mPortalBoxBRenderItem.TexTransform = XMMatrixIdentity();    // unused
  mPortalBoxBRenderItem.ObjCBIndex = 3;
  mPortalBoxBRenderItem.Mat = &mMaterials["room"];            // unused
  mPortalBoxBRenderItem.Geo = &mGeometries["shapeGeo"];
  mPortalBoxBRenderItem.PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
  const SubmeshGeometry& portalBoxBSubmesh = mPortalBoxBRenderItem.Geo->DrawArgs["portalBox"];
  mPortalBoxBRenderItem.IndexCount = portalBoxBSubmesh.IndexCount;
  mPortalBoxBRenderItem.StartIndexLocation = portalBoxBSubmesh.StartIndexLocation;
  mPortalBoxBRenderItem.BaseVertexLocation = portalBoxBSubmesh.BaseVertexLocation;
}

void PortalsApp::BuildFrameResources() {
  for (int i = 0; i < gNumFrameResources; ++i) {
    mFrameResources.push_back(std::make_unique<FrameResource>(
        md3dDevice.Get(), /*objectCount=*/4, NUM_CLIP_PLANE_CBS, NUM_WORLD2_CBS,
        /*passCount=*/1 + 2 * PORTAL_ITERATIONS,
        /*materialCount=*/static_cast<UINT>(mMaterials.size())));
  }
}

void PortalsApp::BuildPSOs() {
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
  ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

  // Common settings used by all PSOs.
  psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
  psoDesc.pRootSignature = mRootSignature.Get();
  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = mBackBufferFormat;
  psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
  psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
  psoDesc.DSVFormat = mDepthStencilFormat;

  // default PSO, for rendering room and player
  
  // default PSO, pixels are clipped against a plane, and stencil test pass when >= ref value.
  ID3DBlob* shader = mShaders["defaultVS"].Get();
  psoDesc.VS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  shader = mShaders["defaultClipPS"].Get();
  psoDesc.PS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.StencilEnable = true;
  psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
      &psoDesc, IID_PPV_ARGS(&mPSOs["defaultClip"])));

  // default PSO, pixels are clipped against two planes, and stencil test pass when >= ref value.
  shader = mShaders["defaultVS"].Get();
  psoDesc.VS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  shader = mShaders["defaultClipTwicePS"].Get();
  psoDesc.PS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.StencilEnable = true;
  psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
      &psoDesc, IID_PPV_ARGS(&mPSOs["defaultClipTwice"])));
  
  // default PSO with portal hole and textures rendered,
  // pixels are clipped against a plane, and stencil test pass when >= ref value.
  shader = mShaders["defaultPortalsVS"].Get();
  psoDesc.VS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  shader = mShaders["defaultPortalsClipPS"].Get();
  psoDesc.PS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.StencilEnable = true;
  psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
      &psoDesc, IID_PPV_ARGS(&mPSOs["defaultPortalsClip"])));

  // portalBox PSO, for rendering a box behind a portal hole to stencil.

  // portalBox PSO with stencil test always passes and replaces with ref value.
  shader = mShaders["portalBoxVS"].Get();
  psoDesc.VS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  shader = mShaders["portalBoxPS"].Get();
  psoDesc.PS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.StencilEnable = true;
  psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  psoDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
  ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
      &psoDesc, IID_PPV_ARGS(&mPSOs["portalBoxStencilSet"])));

  // portalbox PSO with pixels clipped against a plane, stencil test pass when >= ref value,
  // and increments stencil values.
  shader = mShaders["portalBoxVS"].Get();
  psoDesc.VS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  shader = mShaders["portalBoxClipPS"].Get();
  psoDesc.PS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.StencilEnable = true;
  psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  psoDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
  ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
      &psoDesc, IID_PPV_ARGS(&mPSOs["portalBoxStencilIncr"])));

  // portalbox PSO with stencil test pass when >= ref value, disable depth test, and write
  // max depth value to depth buffer.
  shader = mShaders["portalBoxClearDepthVS"].Get();
  psoDesc.VS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  shader = mShaders["portalBoxPS"].Get();
  psoDesc.PS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  psoDesc.DepthStencilState.StencilEnable = true;
  psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
      &psoDesc, IID_PPV_ARGS(&mPSOs["portalBoxClearDepth"])));

  // portalbox PSO with stencil test pass when >= ref value, disable depth test, and zero stencil
  // values. Does not write any colors to render target.
  shader = mShaders["portalBoxVS"].Get();
  psoDesc.VS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  shader = mShaders["portalBoxPS"].Get();
  psoDesc.PS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  psoDesc.DepthStencilState.StencilEnable = true;
  psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  psoDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_ZERO;
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0;
  ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
      &psoDesc, IID_PPV_ARGS(&mPSOs["portalBoxDepthAlwaysStencilZero"])));
}

void PortalsApp::OnResize() {
  D3DApp::OnResize();

  mLeftCamera.SetAspect(AspectRatio());
  mRightCamera.SetAspect(AspectRatio());
}

void PortalsApp::Update(float dt) {
  // Cycle through the circular frame resource array.
  mCurrentFrameResourceIndex = (mCurrentFrameResourceIndex + 1) % gNumFrameResources;
  mCurrentFrameResource = mFrameResources[mCurrentFrameResourceIndex].get();

  // Has the GPU finished processing the commands of the current frame resource?
  // If not, wait until the GPU has completed commands up to this fence point.
  if (mCurrentFrameResource->Fence != 0 &&
      mFence->GetCompletedValue() < mCurrentFrameResource->Fence) {
    HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
    ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFrameResource->Fence, eventHandle));
    WaitForSingleObject(eventHandle, INFINITE);
    CloseHandle(eventHandle);
  }

  // Portals cannot be changed if either portal intersects the player or the spectator camera
  bool modifyPortal = (!mPlayerIntersectPortalA && !mPlayerIntersectPortalB) &&
    !mPortalA.IntersectSphereFromFront(
        mLeftCamera.GetPosition(), mLeftCamera.GetBoundingSphereRadius() + 0.001f) &&
    !mPortalB.IntersectSphereFromFront(
        mLeftCamera.GetPosition(), mLeftCamera.GetBoundingSphereRadius() + 0.001f);

  OnKeyboardInput(dt, modifyPortal);
  UpdateObjectCBs();
  UpdateMaterialBuffer();
  UpdateFrameCB();

  XMFLOAT3 zero(0.0f, 0.0f, 0.0f);
  const float clipPlaneOffest = -0.001f;
  // Neither planes clip anything.
  UpdateClipPlaneCB(CLIP_PLANE_DUMMY_CB_INDEX, zero, zero, -1.0f, zero, zero, -1.0f);
  // Clip plane 1 is portal A's plane, clip plane 2 is portal B's plane.
  UpdateClipPlaneCB(
      CLIP_PLANE_PORTAL_A_B_CB_INDEX, mPortalA.GetPosition(), mPortalA.GetNormal(),
      clipPlaneOffest, mPortalB.GetPosition(), mPortalB.GetNormal(), clipPlaneOffest);
  // Clip plane 1 is portal B's plane, clip plane 2 is portal A's plane.
  UpdateClipPlaneCB(
      CLIP_PLANE_PORTAL_B_A_CB_INDEX, mPortalB.GetPosition(), mPortalB.GetNormal(),
      clipPlaneOffest, mPortalA.GetPosition(), mPortalA.GetNormal(), clipPlaneOffest);

  UpdateWorld2CB(WORLD2_IDENTITY_CB_INDEX, XMMatrixIdentity());
  UpdateWorld2CB(WORLD2_PORTAL_A_TO_B_CB_INDEX, mPortalAToB);
  UpdateWorld2CB(WORLD2_PORTAL_B_TO_A_CB_INDEX, mPortalBToA);
}

void PortalsApp::Draw(float dt) {
  ComPtr<ID3D12CommandAllocator> cmdListAlloc = mCurrentFrameResource->CmdListAlloc;

  // Reuse the memory associated with command recording.
  // We can only reset when the associated command lists have finished execution on the GPU.
  ThrowIfFailed(cmdListAlloc->Reset());

  // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
  // Reusing the command list reuses memory.
  ThrowIfFailed(
      mCommandList->Reset(cmdListAlloc.Get(), mPSOs["defaultPortalsClip"].Get()));

  // Indicate a state transition on the resource usage.
  mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
    D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

  mCommandList->RSSetViewports(1, &mScreenViewport);
  mCommandList->RSSetScissorRects(1, &mScissorRect);

  // Clear the back buffer.
  mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::SkyBlue, 0, nullptr);
  // Clear depth and stencil buffers.
  mCommandList->ClearDepthStencilView(
    DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

  // Specify the buffers we are going to render to.
  mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

  ID3D12DescriptorHeap* descriptorHeap = mSrvDescriptorHeap.Get();
  mCommandList->SetDescriptorHeaps(1, &descriptorHeap);

  mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

  // Bind all the materials used in this scene.  For structured buffers, we can bypass the heap and 
  // set as a root descriptor.
  mCommandList->SetGraphicsRootShaderResourceView(
      SRV_MATERIAL_DATA_ROOT_INDEX,
      mCurrentFrameResource->MaterialBuffer.Resource()->GetGPUVirtualAddress());

  // Bind room and player textures to gTextureMaps[2].
  CD3DX12_GPU_DESCRIPTOR_HANDLE srvDescriptor(
      mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
  mCommandList->SetGraphicsRootDescriptorTable(DT_TEXTURE_MAPS_ROOT_INDEX, srvDescriptor);

  // Bind portalA and portalB textures to gPortalADiffuseMap and gPortalBDiffuseMap.
  srvDescriptor.Offset(2, mCbvSrvUavDescriptorSize);
  mCommandList->SetGraphicsRootDescriptorTable(DT_PORTAL_MAPS_ROOT_INDEX, srvDescriptor);

  // Bind per-frame constant buffer.
  mCommandList->SetGraphicsRootConstantBufferView(
      CB_PER_FRAME_ROOT_INDEX, mCurrentFrameResource->FrameCB.Resource()->GetGPUVirtualAddress());

  
  const UINT portalAStencilRef = 2;
  const UINT portalBStencilRef = 1;
  assert(portalAStencilRef > portalBStencilRef);
  
  const UINT portalAIterations = 3;
  const UINT portalBIterations = 3;

  // Compute per-pass constant buffer values for all iterations.

  const XMMATRIX viewProj = mLeftCamera.GetViewMatrix() * mLeftCamera.GetProjMatrix();
  const XMFLOAT3 eyePosW = mLeftCamera.GetPosition();
  const float distDilation = 1.0f / mLeftCamera.GetViewScale();
  const float radiusAoverB = mPortalA.GetPhysicalRadius() / mPortalB.GetPhysicalRadius();

  UpdatePassCB(0, viewProj, eyePosW, distDilation);

  const UINT portalACBIndexBase = 1;
  const UINT portalBCBIndexBase = portalACBIndexBase + portalAIterations;
  // Compute per-pass constant buffer values for rendering inside portal A.
  XMMATRIX virtualViewProj = viewProj;
  XMVECTOR virtualEyePosWH = XMVectorSet(eyePosW.x, eyePosW.y, eyePosW.z, 1.0f);
  float virtualDistDilation = distDilation;
  for (UINT i = portalACBIndexBase; i < portalACBIndexBase + portalAIterations; ++i) {
    virtualViewProj = mPortalBToA * virtualViewProj;
    virtualEyePosWH = XMVector4Transform(virtualEyePosWH, mPortalAToB);
    virtualDistDilation *= radiusAoverB;

    XMFLOAT3 virtualEyePosW;
    DirectX::XMStoreFloat3(&virtualEyePosW, virtualEyePosWH);
    UpdatePassCB(i, virtualViewProj, virtualEyePosW, virtualDistDilation);
  }
  // Compute per-pass constant buffer values for rendering inside portal B.
  virtualViewProj = viewProj;
  virtualEyePosWH = XMVectorSet(eyePosW.x, eyePosW.y, eyePosW.z, 1.0f);
  virtualDistDilation = distDilation;
  for (UINT i = portalBCBIndexBase; i < portalBCBIndexBase + portalBIterations; ++i) {
    virtualViewProj = mPortalAToB * virtualViewProj;
    virtualEyePosWH = XMVector4Transform(virtualEyePosWH, mPortalBToA);
    virtualDistDilation /= radiusAoverB;

    XMFLOAT3 virtualEyePosW;
    DirectX::XMStoreFloat3(&virtualEyePosW, virtualEyePosWH);
    UpdatePassCB(i, virtualViewProj, virtualEyePosW, virtualDistDilation);
  }

  // Draw room without clipping or stencil-rejecting anything (clip plane is set to dummy plane
  // and stencil buffer is all 0s initially, so stencil test will always pass).

  // Set clip plane to no clipping.
  mCommandList->SetGraphicsRootConstantBufferView(
      CB_CLIP_PLANE_ROOT_INDEX,
      mCurrentFrameResource->ClipPlaneCB.GetResourceGPUVirtualAddress(
          CLIP_PLANE_DUMMY_CB_INDEX));
  // Set per-pass constant buffer to unmodified view space.
  mCommandList->SetGraphicsRootConstantBufferView(
      CB_PER_PASS_ROOT_INDEX,
      mCurrentFrameResource->PassCB.GetResourceGPUVirtualAddress(0));
  // Set world2 matrix to identity matrix.
  mCommandList->SetGraphicsRootConstantBufferView(
      CB_WORLD2_ROOT_INDEX,
      mCurrentFrameResource->World2CB.GetResourceGPUVirtualAddress(
          WORLD2_IDENTITY_CB_INDEX));
  mCommandList->OMSetStencilRef(0);

  // Draw real room
  DrawRenderItem(mCommandList.Get(), &mRoomRenderItem);

  // Draw real player or player halves.
  if (mPlayerIntersectPortalA) {
    DrawIntersectingPlayerRealHalves(
        CLIP_PLANE_PORTAL_A_B_CB_INDEX, CLIP_PLANE_PORTAL_B_A_CB_INDEX, WORLD2_PORTAL_A_TO_B_CB_INDEX);
  } else if (mPlayerIntersectPortalB) {
    DrawIntersectingPlayerRealHalves(
        CLIP_PLANE_PORTAL_B_A_CB_INDEX, CLIP_PLANE_PORTAL_A_B_CB_INDEX, WORLD2_PORTAL_B_TO_A_CB_INDEX);
  } else {
    mCommandList->SetPipelineState(mPSOs["defaultClip"].Get());
    DrawRenderItem(mCommandList.Get(), &mPlayerRenderItem);
  }

  // Draw portal boxes for both portals to cover their holes. This is done before rendering the
  // insides of either portal to prevent pixels of portal box A appearing inside an uncovered
  // portal B hole. While rendering the inside of portal A, those pixels might be rendered to with
  // a depth closer than portal B. Then, when portal B's box is rendered, those pixels will remain
  // "in front" which means they won't be properly marked in the stencil as being inside portal B.
  // Note: portal boxes must be drawn after player so that portal pixels behind the player are not
  // marked in the stencil buffer.
  mCommandList->SetPipelineState(mPSOs["portalBoxStencilSet"].Get());
  mCommandList->OMSetStencilRef(portalAStencilRef);
  DrawRenderItem(mCommandList.Get(), &mPortalBoxARenderItem);
  mCommandList->OMSetStencilRef(portalBStencilRef);
  DrawRenderItem(mCommandList.Get(), &mPortalBoxBRenderItem);

  // At this point, the stencil buffer is 2 inside portal A, 1 inside portal B, and 0 everywhere
  // else. First, the inside of portal A is rendered using stencil tests >=2, >=3, ... .  Then,
  // portal box A is rendered again with depth-test-always, stencil test >=2, and zeroing the
  // stencil buffer. This shoud leave the stencil buffer 0 everywhere except inside portal B, where
  // it's still 1. Finally, the inside of portal B is rendered using stencil tests >=1, >=2, ...

  if (mPlayerIntersectPortalA || mPlayerIntersectPortalB) {
    // Render insides of portal A and part of player sticking out of portal A.
    DrawRoomsAndIntersectingPlayersForPortal(
        portalAStencilRef, &mPortalBoxARenderItem, portalACBIndexBase, portalAIterations,
        CLIP_PLANE_PORTAL_A_B_CB_INDEX, CLIP_PLANE_PORTAL_B_A_CB_INDEX, mPlayerIntersectPortalA,
        WORLD2_PORTAL_A_TO_B_CB_INDEX, WORLD2_PORTAL_B_TO_A_CB_INDEX);

    // Clear stencil buffer for pixels inside portal A.
    DrawPortalBoxToCoverDepthHoleAndZeroStencil(portalAStencilRef, &mPortalBoxARenderItem);

    // Render insides of portal B and part of player sticking out of portal B.
    DrawRoomsAndIntersectingPlayersForPortal(
        portalBStencilRef, &mPortalBoxBRenderItem, portalBCBIndexBase, portalBIterations,
        CLIP_PLANE_PORTAL_B_A_CB_INDEX, CLIP_PLANE_PORTAL_A_B_CB_INDEX, mPlayerIntersectPortalB,
        WORLD2_PORTAL_B_TO_A_CB_INDEX, WORLD2_PORTAL_A_TO_B_CB_INDEX);
  } else {
    // Render insides of portal A.
    DrawRoomAndPlayerIterations(
        portalAStencilRef, &mPortalBoxARenderItem, portalACBIndexBase, portalAIterations,
        CLIP_PLANE_PORTAL_B_A_CB_INDEX, true);

    // Clear stencil buffer for pixels inside portal A and cover up its hole in the depth buffer.
    // This way, the first portal B box can't appear "in front" of portal A due to portal A's depth
    // hole, and stencil tests for rendering inside portal B won't pass for any pixels inside
    // portal A.
    DrawPortalBoxToCoverDepthHoleAndZeroStencil(portalAStencilRef, &mPortalBoxARenderItem);

    // Render insides of portal B.
    DrawRoomAndPlayerIterations(
        portalBStencilRef, &mPortalBoxBRenderItem, portalBCBIndexBase, portalBIterations,
        CLIP_PLANE_PORTAL_A_B_CB_INDEX, true);

  }


  // Indicate a state transition on the resource usage.
  mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
      D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

  // Done recording commands.
  ThrowIfFailed(mCommandList->Close());

  // Add the command list to the queue for execution.
  ID3D12CommandList* cmdList = mCommandList.Get();
  mCommandQueue->ExecuteCommandLists(1, &cmdList);

  // Swap the back and front buffers
  ThrowIfFailed(mSwapChain->Present(0, 0));
  mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

  // Advance the fence value to mark commands up to this fence point.
  mCurrentFrameResource->Fence = ++mCurrentFence;

  // Add an instruction to the command queue to set a new fence point. 
  // Because we are on the GPU timeline, the new fence point won't be 
  // set until the GPU finishes processing all the commands prior to this Signal().
  mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

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

  // Portal A
  // Initial radius
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f", &a);
  mPortalA.SetIntendedPhysicalRadius(a);
  // Initial position
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f %f %f", &a, &b, &c);
  mPortalA.SetPosition(XMFLOAT3(a, b, c));
  // Initial normal
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f %f %f", &a, &b, &c);
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f %f %f", &d, &e, &f);
  mPortalA.SetNormalAndUp(XMFLOAT3(a, b, c), XMFLOAT3(d, e, f));

  // Portal B
  // Initial radius
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f", &a);
  mPortalB.SetIntendedPhysicalRadius(a);
  // Initial position
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f %f %f", &a, &b, &c);
  mPortalB.SetPosition(XMFLOAT3(a, b, c));
  // Initial normal
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f %f %f", &a, &b, &c);
  GetNextDataLine(&ifs, &line);
  sscanf_s(line.c_str(), "%f %f %f", &d, &e, &f);
  mPortalB.SetNormalAndUp(XMFLOAT3(a, b, c), XMFLOAT3(d, e, f));

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

void PortalsApp::OnKeyboardInput(float dt, bool modifyPortal) {
  // Update which portal to control
  if (GetAsyncKeyState('O') & 0x8000) {
    mCurrentPortal = &mPortalA;
    mOtherPortal = &mPortalB;
    mCurrentPortalBoxRenderItem = &mPortalBoxARenderItem;
  } else if (GetAsyncKeyState('B') & 0x8000) {
    mCurrentPortal = &mPortalB;
    mOtherPortal = &mPortalA;
    mCurrentPortalBoxRenderItem = &mPortalBoxBRenderItem;
  }

  // Update which camera to control
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
    SpherePath::MoveCameraAlongPathIterative(
        *mCurrentCamera, dir, speed * dt, mRoom, mPortalA, mPortalB);
    // Update player world matrix and portal intersect flags if this is the camera attached to the
    // player.
    if (mCurrentCamera->GetAttachedTo() == &mPlayer) {
      mPlayerRenderItem.World = mPlayer.GetWorldMatrix();
      mPlayerRenderItem.NumFramesDirty = gNumFrameResources;
      
      mPlayerIntersectPortalA = mPortalA.IntersectSphereFromFront(
          mPlayer.GetPosition(), mPlayer.GetBoundingSphereRadius() + 0.001f);
      mPlayerIntersectPortalB = mPortalB.IntersectSphereFromFront(
          mPlayer.GetPosition(), mPlayer.GetBoundingSphereRadius() + 0.001f);
    }
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
    if (PortalSizeIncreaseUnits != 0.0f) {
      float NewRadius = mCurrentPortal->GetPhysicalRadius() +
          (PortalSizeIncreaseUnits * PORTAL_SIZE_CHANGE_SPEED * dt);
      mCurrentPortal->SetIntendedPhysicalRadius(NewRadius);
    }
    // Update portal box world matrix
    mCurrentPortalBoxRenderItem->World = mCurrentPortal->GetXYScaledPortalToWorldMatrix();
    mCurrentPortalBoxRenderItem->NumFramesDirty = gNumFrameResources;
    // Update portal A-to_B and B-to-A matrices.
    mPortalAToB = Portal::CalculateVirtualizationMatrix(mPortalA, mPortalB);
    mPortalBToA = Portal::CalculateVirtualizationMatrix(mPortalB, mPortalA);
  }
}

void PortalsApp::UpdateMaterialBuffer() {
  for (std::pair<const std::string, PhongMaterial>& e : mMaterials) {
    // Only update the cbuffer data if the constants have changed.  If the cbuffer
    // data changes, it needs to be updated for each FrameResource.
    PhongMaterial* mat = &e.second;
    if (mat->NumFramesDirty > 0) {
      PhongMaterialData matData;
      matData.Diffuse = mat->Diffuse;
      matData.Specular = mat->Specular;
      matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;

      mCurrentFrameResource->MaterialBuffer.CopyData(mat->MatCBIndex, matData);

      mat->NumFramesDirty--;
    }
  }
}

void PortalsApp::UpdateObjectCBs() {

  RenderItem* items[] = {
      &mRoomRenderItem, &mPlayerRenderItem, &mPortalBoxARenderItem, &mPortalBoxBRenderItem };
  for (RenderItem* item : items) {
    // Only update the buffer data if the constants have changed. This needs to be tracked per
    // frame resource.
    if (item->NumFramesDirty > 0) {
      ObjectConstants objConstants;
      XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(item->World));
      XMStoreFloat4x4(&objConstants.WorldInvTranspose, XMMatrixTranspose(
          MathHelper::InverseTranspose(item->World)));
      XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(item->TexTransform));
      objConstants.MaterialIndex = item->Mat->MatCBIndex;

      mCurrentFrameResource->ObjectCB.CopyData(item->ObjCBIndex, objConstants);

      item->NumFramesDirty--;
    }
  }
}

void PortalsApp::UpdateClipPlaneCB(
    int index, const XMFLOAT3& position, const XMFLOAT3& normal, float offset,
    const XMFLOAT3& position2, const XMFLOAT3& normal2, float offset2) {
  ClipPlaneConstants clipPlaneCB;
  clipPlaneCB.ClipPlaneNormal = normal;
  clipPlaneCB.ClipPlaneOffset = XMFloat3Dot(position, normal) + offset;
  clipPlaneCB.ClipPlane2Normal = normal2;
  clipPlaneCB.ClipPlane2Offset = XMFloat3Dot(position2, normal2) + offset2;

  mCurrentFrameResource->ClipPlaneCB.CopyData(index, clipPlaneCB);
}

void PortalsApp::UpdateWorld2CB(int index, const XMMATRIX& world2) {
  World2Constants world2CB;
  XMStoreFloat4x4(&world2CB.World2, XMMatrixTranspose(world2));
  XMStoreFloat4x4(&world2CB.World2InvTranspose, XMMatrixTranspose(
      MathHelper::InverseTranspose(world2)));

  mCurrentFrameResource->World2CB.CopyData(index, world2CB);
}

void PortalsApp::UpdatePassCB(
    int index, const XMMATRIX& viewProj, const XMFLOAT3& eyePosW, float distDilation) {
  PassConstants passCB;
  XMStoreFloat4x4(&passCB.ViewProj, XMMatrixTranspose(viewProj));
  passCB.EyePosW = eyePosW;
  passCB.DistDilation = distDilation;

  mCurrentFrameResource->PassCB.CopyData(index, passCB);
}

void PortalsApp::UpdateFrameCB() {
  FrameConstants frameCB;
  XMStoreFloat4x4(&frameCB.PortalA, XMMatrixTranspose(mPortalA.GetXYScaledWorldToPortalMatrix()));
  XMStoreFloat4x4(&frameCB.PortalB, XMMatrixTranspose(mPortalB.GetXYScaledWorldToPortalMatrix()));
  frameCB.AmbientLight = mAmbientLight;
  for (int i = 0; i < NUM_LIGHTS; ++i) {
    frameCB.Lights[i].Strength = mDirLights[i].Strength;
    frameCB.Lights[i].Direction = mDirLights[i].Direction;
  }

  mCurrentFrameResource->FrameCB.CopyData(0, frameCB);
}

void PortalsApp::DrawRenderItem(
    ID3D12GraphicsCommandList* cmdList, RenderItem* ri, bool sameAsPrevious) {
  if (!sameAsPrevious) {
    cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
    cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
    cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

    cmdList->SetGraphicsRootConstantBufferView(
      CB_PER_OBJECT_ROOT_INDEX,
      mCurrentFrameResource->ObjectCB.GetResourceGPUVirtualAddress(ri->ObjCBIndex));
  }
  cmdList->DrawIndexedInstanced(
      ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
}

void PortalsApp::DrawIntersectingPlayerRealHalves(
    int clipPlanePortalCBIndex, int clipPlaneOtherPortalCBIndex, int world2ThisToOtherCBIndex) {
  // Assume stencil ref is already set to 0.

  mCommandList->SetPipelineState(mPSOs["defaultClip"].Get());

  // Set clip plane to this portal.
  mCommandList->SetGraphicsRootConstantBufferView(
      CB_CLIP_PLANE_ROOT_INDEX,
      mCurrentFrameResource->ClipPlaneCB.GetResourceGPUVirtualAddress(clipPlanePortalCBIndex));
  // Draw larger half of player.
  DrawRenderItem(mCommandList.Get(), &mPlayerRenderItem);

  // Set clip plane to other portal.
  mCommandList->SetGraphicsRootConstantBufferView(
      CB_CLIP_PLANE_ROOT_INDEX,
      mCurrentFrameResource->ClipPlaneCB.GetResourceGPUVirtualAddress(clipPlaneOtherPortalCBIndex));
  // Set world2 matrix to this-to-other.
  mCommandList->SetGraphicsRootConstantBufferView(
      CB_WORLD2_ROOT_INDEX,
      mCurrentFrameResource->World2CB.GetResourceGPUVirtualAddress(
          world2ThisToOtherCBIndex));
  // Draw smaller half of player.
  DrawRenderItem(mCommandList.Get(), &mPlayerRenderItem);
  // Restore world2 matrix to identity.
  mCommandList->SetGraphicsRootConstantBufferView(
      CB_WORLD2_ROOT_INDEX,
      mCurrentFrameResource->World2CB.GetResourceGPUVirtualAddress(
          WORLD2_IDENTITY_CB_INDEX));
}

void PortalsApp::DrawRoomAndPlayerIterations(
    UINT stencilRef, RenderItem* portalBoxRi, int CBIndexBase, int numIterations,
    int clipPlaneOtherPortalCBIndex, bool drawPlayers) {
  // Set per-pass constant buffer to unmodified view space.
  mCommandList->SetGraphicsRootConstantBufferView(
      CB_PER_PASS_ROOT_INDEX,
      mCurrentFrameResource->PassCB.GetResourceGPUVirtualAddress(0));
  
  // Set clip plane (not used by portalBoxClearDepth).
  mCommandList->SetGraphicsRootConstantBufferView(
      CB_CLIP_PLANE_ROOT_INDEX,
      mCurrentFrameResource->ClipPlaneCB.GetResourceGPUVirtualAddress(clipPlaneOtherPortalCBIndex));

  int passCBIndex = CBIndexBase;
  for (int i = 0; i < numIterations; ++i, ++stencilRef, ++passCBIndex) {
    mCommandList->OMSetStencilRef(stencilRef);

    // Draw portal box to clear depth values inside portal hole.
    mCommandList->SetPipelineState(mPSOs["portalBoxClearDepth"].Get());
    DrawRenderItem(mCommandList.Get(), portalBoxRi, i > 0);

    // Advance per-pass constant buffer.
    mCommandList->SetGraphicsRootConstantBufferView(
        CB_PER_PASS_ROOT_INDEX,
        mCurrentFrameResource->PassCB.GetResourceGPUVirtualAddress(passCBIndex));

    // Draw room
    mCommandList->SetPipelineState(mPSOs["defaultPortalsClip"].Get());
    DrawRenderItem(mCommandList.Get(), &mRoomRenderItem);

    if (drawPlayers) {
      mCommandList->SetPipelineState(mPSOs["defaultClip"].Get());
      DrawRenderItem(mCommandList.Get(), &mPlayerRenderItem);
    }

    // Draw portal box to increment stencil values inside portal hole.
    mCommandList->SetPipelineState(mPSOs["portalBoxStencilIncr"].Get());
    DrawRenderItem(mCommandList.Get(), portalBoxRi);
  }
}

void PortalsApp::DrawPlayerIterations(
    UINT stencilRef, int CBIndexBase, int numIterations, const std::string& psoName) {
  mCommandList->SetPipelineState(mPSOs[psoName].Get());

  int passCBIndex = CBIndexBase;
  for (int i = 0; i < numIterations; ++i, ++stencilRef, ++passCBIndex) {
    mCommandList->OMSetStencilRef(stencilRef);

    // Set per-pass constant buffer.
    mCommandList->SetGraphicsRootConstantBufferView(
        CB_PER_PASS_ROOT_INDEX,
        mCurrentFrameResource->PassCB.GetResourceGPUVirtualAddress(passCBIndex));

    // Draw player
    DrawRenderItem(mCommandList.Get(), &mPlayerRenderItem, i > 0);
  }
}

void PortalsApp::DrawRoomsAndIntersectingPlayersForPortal(
    UINT stencilRef, RenderItem* portalBoxRi, int CBIndexBase, int numIterations,
    int clipPlanePortalCBIndex, int clipPlaneOtherPortalCBIndex, bool playerIntersectPortal,
    int world2ThisToOtherCBIndex, int world2OtherToThisCBIndex) {
  // Draw rooms inside portal
  DrawRoomAndPlayerIterations(
      stencilRef, portalBoxRi, CBIndexBase, numIterations, clipPlaneOtherPortalCBIndex, false);

  // Set clip plane 1 to this portal and clip plane 2 to other portal (using defaultClipTwice PSO)
  mCommandList->SetGraphicsRootConstantBufferView(
      CB_CLIP_PLANE_ROOT_INDEX,
      mCurrentFrameResource->ClipPlaneCB.GetResourceGPUVirtualAddress(
          clipPlanePortalCBIndex));
  if (playerIntersectPortal) {
    // Draw larger half of players
    DrawPlayerIterations(stencilRef, CBIndexBase, numIterations, "defaultClipTwice");
  } else {
    // Set world2 matrix to other-to-this.
    mCommandList->SetGraphicsRootConstantBufferView(
        CB_WORLD2_ROOT_INDEX,
        mCurrentFrameResource->World2CB.GetResourceGPUVirtualAddress(
            world2OtherToThisCBIndex));
    // Draw smaller half of players.
    DrawPlayerIterations(stencilRef, CBIndexBase, numIterations, "defaultClipTwice");
    // Restore world2 matrix to identity.
    mCommandList->SetGraphicsRootConstantBufferView(
        CB_WORLD2_ROOT_INDEX,
        mCurrentFrameResource->World2CB.GetResourceGPUVirtualAddress(
            WORLD2_IDENTITY_CB_INDEX));
  }
    
  // Set clip plane to other portal (using defaultClip PSO)
  mCommandList->SetGraphicsRootConstantBufferView(
      CB_CLIP_PLANE_ROOT_INDEX,
      mCurrentFrameResource->ClipPlaneCB.GetResourceGPUVirtualAddress(
          clipPlaneOtherPortalCBIndex));
  if (playerIntersectPortal) {
    // Set world2 matrix to this-to-other.
    mCommandList->SetGraphicsRootConstantBufferView(
        CB_WORLD2_ROOT_INDEX,
        mCurrentFrameResource->World2CB.GetResourceGPUVirtualAddress(
            world2ThisToOtherCBIndex));
    // Draw smaller half of players.
    DrawPlayerIterations(stencilRef, CBIndexBase, numIterations, "defaultClip");
    // Restore world2 matrix to identity.
    mCommandList->SetGraphicsRootConstantBufferView(
        CB_WORLD2_ROOT_INDEX,
        mCurrentFrameResource->World2CB.GetResourceGPUVirtualAddress(
            WORLD2_IDENTITY_CB_INDEX));
  } else {
    // Draw larger half of players.
    DrawPlayerIterations(stencilRef, CBIndexBase, numIterations, "defaultClip");
  }
}

void PortalsApp::DrawPortalBoxToCoverDepthHoleAndZeroStencil(
    UINT stencilRef, RenderItem* portalBoxRi) {
  // No need to reset clip plane since portalBoxDepthAlwaysStencilZero PSO doesn't use it.
  // Set per-pass constant buffer to unmodified view space.
  mCommandList->SetGraphicsRootConstantBufferView(
      CB_PER_PASS_ROOT_INDEX,
      mCurrentFrameResource->PassCB.GetResourceGPUVirtualAddress(0));
  mCommandList->OMSetStencilRef(stencilRef);
  mCommandList->SetPipelineState(mPSOs["portalBoxDepthAlwaysStencilZero"].Get());
  DrawRenderItem(mCommandList.Get(), portalBoxRi);
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