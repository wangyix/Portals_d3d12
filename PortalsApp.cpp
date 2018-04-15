#include "PortalsApp.h"

#include "GeometryGenerator.h"
#include "SpherePath.h"

namespace {
  
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

  CD3DX12_ROOT_PARAMETER rootParameters[7];
  // Order from most frequent to least frequent.
  rootParameters[0].InitAsConstantBufferView(0);      // cbPerObject
  rootParameters[1].InitAsConstantBufferView(1);      // cbPass
  rootParameters[2].InitAsConstantBufferView(2);      // cbClipPlane
  rootParameters[3].InitAsConstantBufferView(3);      // cbFrame
  rootParameters[4].InitAsShaderResourceView(0, 1);   // gMaterialData
  // gPortalADiffuseMap, gPortalBDiffuseMap
  rootParameters[5].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
  // gTextureMaps
  rootParameters[6].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

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
  std::string numLightsStr = std::to_string(NUM_LIGHTS);
  D3D_SHADER_MACRO defines[] = {
      { "NUM_LIGHTS", numLightsStr.c_str() },
      { nullptr, nullptr },
      { nullptr, nullptr },
      { nullptr, nullptr },
      { nullptr, nullptr },
      { nullptr, nullptr } };
  mShaders["portalBoxVS"] = d3dUtil::CompileShader(L"fx/PortalBox.hlsl", defines, "VS", "vs_5_1");
  mShaders["portalBoxPS"] = d3dUtil::CompileShader(L"fx/PortalBox.hlsl", defines, "PS", "ps_5_1");
  defines[1] = { "CLEAR_DEPTH", nullptr };
  mShaders["portalBoxClearDepthVS"] = d3dUtil::CompileShader(L"fx/PortalBox.hlsl", defines, "VS", "vs_5_1");
  defines[1] = { "CLIP_PLANE", nullptr };
  mShaders["portalBoxClipPS"] = d3dUtil::CompileShader(L"fx/PortalBox.hlsl", defines, "PS", "ps_5_1");

  std::string portalTexRadRatioStr = std::to_string(PORTAL_TEX_RAD_RATIO);
  defines[1] = { "PORTAL_TEX_RAD_RATIO", portalTexRadRatioStr.c_str() };
  mShaders["defaultVS"] = d3dUtil::CompileShader(L"fx/Default.hlsl", defines, "VS", "vs_5_1");
  mShaders["defaultPS"] = d3dUtil::CompileShader(L"fx/Default.hlsl", defines, "PS", "ps_5_1");
  defines[2] = { "DRAW_PORTALS", nullptr };
  mShaders["defaultPortalsVS"] = d3dUtil::CompileShader(L"fx/Default.hlsl", defines, "VS", "vs_5_1");
  mShaders["defaultPortalsPS"] = d3dUtil::CompileShader(L"fx/Default.hlsl", defines, "PS", "ps_5_1");
  defines[3] = { "CLIP_PLANE", nullptr };
  mShaders["defaultPortalsClipPS"] = d3dUtil::CompileShader(L"fx/Default.hlsl", defines, "PS", "ps_5_1");

  mInputLayout = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
  };
}
#define QUAD 0
void PortalsApp::BuildShapeGeometry() {
#if QUAD == 0
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
#else

  {
    std::vector<Vertex> vertices = {
      { { -0.5f, -0.5f, 0.5f },{ 0.0f, 0.0f, -1.0f },{ 0.0f, 0.0f } },
      { { 0.5f, -0.5f, 0.5f },{ 0.0f, 0.0f, -1.0f },{ 1.0f, 0.0f } },
      { { 0.5f, 0.5f, 0.5f },{ 0.0f, 0.0f, -1.0f },{ 1.0f, 1.0f } },
      { { -0.5f, 0.5f, 0.5f },{ 0.0f, 0.0f, -1.0f },{ 0.0f, 1.0f } }
    };
    std::vector<uint16_t> indices = {
      0,2,1,
      0,3,2 };

    SubmeshGeometry submesh;
    submesh.IndexCount = 6;
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    const UINT vbByteSize = vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = indices.size() * sizeof(std::uint16_t);
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
    geo->DrawArgs["quad"] = submesh;
  }
#endif
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
#if QUAD == 0
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
#else

  mRoomRenderItem.World = XMMatrixScaling(1.5f, 1.5f, 1.0f);
  mRoomRenderItem.TexTransform = XMMatrixScaling(0.3f, 0.3f, 1.0f)*XMMatrixTranslation(0.0f, 0.125f, 0.0f);
  mRoomRenderItem.ObjCBIndex = 0;
  mRoomRenderItem.Mat = &mMaterials["room"];
  mRoomRenderItem.Geo = &mGeometries["shapeGeo"];
  mRoomRenderItem.PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
  const SubmeshGeometry& roomSubMesh = mRoomRenderItem.Geo->DrawArgs["quad"];
  mRoomRenderItem.IndexCount = roomSubMesh.IndexCount;
  mRoomRenderItem.StartIndexLocation = roomSubMesh.StartIndexLocation;
  mRoomRenderItem.BaseVertexLocation = roomSubMesh.BaseVertexLocation;
#endif
}

void PortalsApp::BuildFrameResources() {
  for (int i = 0; i < gNumFrameResources; ++i) {
    mFrameResources.push_back(std::make_unique<FrameResource>(
        md3dDevice.Get(), /*clipPlanesCount=*/2, /*passCount=*/1 + 2 * PORTAL_ITERATIONS,
        /*objectCount=*/4, /*materialCount=*/static_cast<UINT>(mMaterials.size())));
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
  
  // default PSO with no special settings.
  ID3DBlob* shader = mShaders["defaultVS"].Get();
  psoDesc.VS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  shader = mShaders["defaultPS"].Get();
  psoDesc.PS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
      &psoDesc, IID_PPV_ARGS(&mPSOs["default"])));

  // default PSO with portal hole and textures rendered.
  shader = mShaders["defaultPortalsVS"].Get();
  psoDesc.VS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  shader = mShaders["defaultPortalsPS"].Get();
  psoDesc.PS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
      &psoDesc, IID_PPV_ARGS(&mPSOs["defaultPortals"])));
  
  // default PSO with pixels clipped against a plane and stencil test pass when equal to ref value.
  shader = mShaders["defaultPortalsVS"].Get();
  psoDesc.VS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  shader = mShaders["defaultPortalsClipPS"].Get();
  psoDesc.PS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.StencilEnable = true;
  psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
  ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
      &psoDesc, IID_PPV_ARGS(&mPSOs["defaultPortalsClipStencilEqual"])));

  // portalBox PSO, for rendering a box behind a portal hole to stencil.
  
  // portalbox PSO that will write ref value to stencil.
  shader = mShaders["portalBoxVS"].Get();
  psoDesc.VS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  shader = mShaders["portalBoxPS"].Get();
  psoDesc.PS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.StencilEnable = true;
  psoDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
  ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
      &psoDesc, IID_PPV_ARGS(&mPSOs["portalBoxStencilSet"])));

  // portalbox PSO with stencil test pass when equal to ref value, disable depth test, and write
  // max depth value to depth buffer.
  shader = mShaders["portalBoxClearDepthVS"].Get();
  psoDesc.VS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  shader = mShaders["portalBoxPS"].Get();
  psoDesc.PS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  psoDesc.DepthStencilState.StencilEnable = true;
  psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
  ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
      &psoDesc, IID_PPV_ARGS(&mPSOs["portalBoxStencilEqualClearDepth"])));

  // portalbox PSO with pixels clipped against a plane, stencil test pass when equal to ref value,
  // and increments stencil values.
  shader = mShaders["portalBoxVS"].Get();
  psoDesc.VS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  shader = mShaders["portalBoxClipPS"].Get();
  psoDesc.PS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.StencilEnable = true;
  psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
  psoDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
  ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
      &psoDesc, IID_PPV_ARGS(&mPSOs["portalBoxStencilEqualIncr"])));
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
    !mPortalA.DiscIntersectSphere(
        mLeftCamera.GetPosition(), mLeftCamera.GetBoundingSphereRadius() + 0.001f) &&
    !mPortalB.DiscIntersectSphere(
        mLeftCamera.GetPosition(), mLeftCamera.GetBoundingSphereRadius() + 0.001f);

  OnKeyboardInput(dt, modifyPortal);
  UpdateObjectCBs();
  UpdateMaterialBuffer();
  UpdateFrameCB();
  UpdateClipPlaneCB(0, &mPortalA);
  UpdateClipPlaneCB(1, &mPortalB);
}

void PortalsApp::Draw(float dt) {
  ComPtr<ID3D12CommandAllocator> cmdListAlloc = mCurrentFrameResource->CmdListAlloc;

  // Reuse the memory associated with command recording.
  // We can only reset when the associated command lists have finished execution on the GPU.
  ThrowIfFailed(cmdListAlloc->Reset());

  // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
  // Reusing the command list reuses memory.
  ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["defaultPortals"].Get()));

  // Indicate a state transition on the resource usage.
  mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
    D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

  mCommandList->RSSetViewports(1, &mScreenViewport);
  mCommandList->RSSetScissorRects(1, &mScissorRect);

  // Clear the back and depth buffer.
  mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::SkyBlue, 0, nullptr);
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
      4, mCurrentFrameResource->MaterialBuffer.Resource()->GetGPUVirtualAddress());

  // Bind room and player textures to gTextureMaps[2].
  CD3DX12_GPU_DESCRIPTOR_HANDLE srvDescriptor(
      mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
  mCommandList->SetGraphicsRootDescriptorTable(6, srvDescriptor);

  // Bind portalA and portalB textures to gPortalADiffuseMap and gPortalBDiffuseMap.
  srvDescriptor.Offset(2, mCbvSrvUavDescriptorSize);
  mCommandList->SetGraphicsRootDescriptorTable(5, srvDescriptor);

  // Bind per-frame constant buffer.
  mCommandList->SetGraphicsRootConstantBufferView(
      3, mCurrentFrameResource->FrameCB.Resource()->GetGPUVirtualAddress()); 





  // Update per-pass constant buffer.
  XMMATRIX virtualViewProj = mLeftCamera.GetViewMatrix() * mLeftCamera.GetProjMatrix();
  XMFLOAT3 virtualEyePosW = mLeftCamera.GetPosition();
  XMVECTOR virtualEyePosWH =
      XMVectorSet(virtualEyePosW.x, virtualEyePosW.y, virtualEyePosW.z, 1.0f);
  float virtualViewScale = mLeftCamera.GetViewScale();
#if QUAD == 0
  UpdatePassCB(0, virtualViewProj, virtualEyePosW, virtualViewScale);
#else
  UpdatePassCB(0, XMMatrixScaling(0.5f, 1.0f, 1.0f)*XMMatrixTranslation(0.2f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -2.0f), 0.5f);  // TEST!!!!!!!!!!!!!!!!!!
#endif
  // Bind per-pass constant buffer.
  UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
  mCommandList->SetGraphicsRootConstantBufferView(
    1, mCurrentFrameResource->PassCB.Resource()->GetGPUVirtualAddress() + 0 * passCBByteSize);

  // Draw room
  DrawRenderItem(mCommandList.Get(), &mRoomRenderItem);

  // Rest of the rendering steps depends on whether or not the player intersects a portal
  if (mPlayerIntersectPortalA || mPlayerIntersectPortalB) {

  } else {
    // Draw player
    mCommandList->SetPipelineState(mPSOs["default"].Get());
    DrawRenderItem(mCommandList.Get(), &mPlayerRenderItem);

    mCommandList->OMSetStencilRef(1);

    // Render portal box A to stencil
    mCommandList->SetPipelineState(mPSOs["portalBoxStencilSet"].Get());
    DrawRenderItem(mCommandList.Get(), &mPortalBoxARenderItem);
    mCommandList->SetPipelineState(mPSOs["portalBoxStencilEqualClearDepth"].Get());
    DrawRenderItem(mCommandList.Get(), &mPortalBoxARenderItem);

    // Bind clip-plane constant buffer to portal B clip plane values.
    UINT clipPlaneCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ClipPlaneConstants));
    mCommandList->SetGraphicsRootConstantBufferView(
        2, mCurrentFrameResource->ClipPlaneCB.Resource()->GetGPUVirtualAddress() +
            1 * clipPlaneCBByteSize);


    const XMMATRIX virtualizeBtoA = Portal::CalculateVirtualizationMatrix(mPortalA, mPortalB);
    const XMMATRIX virtualizeAtoB = Portal::CalculateVirtualizationMatrix(mPortalB, mPortalA);
    const float radiusAoverB = mPortalA.GetPhysicalRadius() / mPortalB.GetPhysicalRadius();

    // Update per-pass constant buffer.
    virtualViewProj = virtualizeBtoA * virtualViewProj;
    virtualEyePosWH = XMVector4Transform(virtualEyePosWH, virtualizeAtoB);
    virtualViewScale /= radiusAoverB;
    XMStoreFloat3(&virtualEyePosW, virtualEyePosWH);
    UpdatePassCB(1, virtualViewProj, virtualEyePosW, virtualViewScale);
    // Bind per-pass constant buffer.
    UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
    mCommandList->SetGraphicsRootConstantBufferView(
        1, mCurrentFrameResource->PassCB.Resource()->GetGPUVirtualAddress() + 1 * passCBByteSize);

    // Render room inside portal A
    mCommandList->SetPipelineState(mPSOs["defaultPortalsClipStencilEqual"].Get());
    DrawRenderItem(mCommandList.Get(), &mRoomRenderItem);
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
    // Update player world matrix if this is the camera attached to the player.
    if (mCurrentCamera->GetAttachedTo() == &mPlayer) {
      mPlayerRenderItem.World = mPlayer.GetWorldMatrix();
      mPlayerRenderItem.NumFramesDirty = gNumFrameResources;
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
  }
}

void PortalsApp::UpdateObjectCBs() {
#if QUAD == 0
  RenderItem* items[] = {
      &mRoomRenderItem, &mPlayerRenderItem, &mPortalBoxARenderItem, &mPortalBoxBRenderItem };
  for (RenderItem* item : items) {
#else
RenderItem* item = &mRoomRenderItem;   // TEST!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  {
#endif
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

void PortalsApp::UpdateClipPlaneCB(int index, const Portal* clipPortal) {
  ClipPlaneConstants clipPlaneCB;
  clipPlaneCB.ClipPlanePosition = clipPortal->GetPosition();
  clipPlaneCB.ClipPlaneNormal = clipPortal->GetNormal();
  clipPlaneCB.ClipPlaneOffset = 0.0f;

  mCurrentFrameResource->ClipPlaneCB.CopyData(index, clipPlaneCB);
}

void PortalsApp::UpdatePassCB(
    int index, const XMMATRIX& viewProj, const XMFLOAT3& eyePosW, float viewScale) {
  PassConstants passCB;
  XMStoreFloat4x4(&passCB.ViewProj, XMMatrixTranspose(viewProj));
  passCB.EyePosW = eyePosW;
  passCB.ViewScale = viewScale;

  mCurrentFrameResource->PassCB.CopyData(index, passCB);
}

void PortalsApp::DrawRenderItem(ID3D12GraphicsCommandList* cmdList, RenderItem* ri) {
  cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
  cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
  cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

  UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
  D3D12_GPU_VIRTUAL_ADDRESS objCBAddress =
      mCurrentFrameResource->ObjectCB.Resource()->GetGPUVirtualAddress() +
      ri->ObjCBIndex * objCBByteSize;
  cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

  cmdList->DrawIndexedInstanced(
      ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
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