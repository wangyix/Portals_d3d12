#include "PortalsApp.h"

#include "GeometryGenerator.h"
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

  mDirLights[0].Strength = XMFLOAT3(1.0f, 1.0f, 1.0f);
  mDirLights[0].Direction = XMFLOAT3(0.57735f, -0.57735f, 0.57735f);
  mDirLights[1].Strength = XMFLOAT3(0.5f, 0.5f, 0.5f);
  mDirLights[1].Direction = XMFLOAT3(-0.57735f, -0.57735f, 0.57735f);
  mDirLights[2].Strength = XMFLOAT3(0.2f, 0.2f, 0.2f);
  mDirLights[2].Direction = XMFLOAT3(0.0f, -0.707f, -0.707f);

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
  LoadTexture("room", L"textures/tile.dds");
  LoadTexture("player", L"textures/stone.dds");

  BuildRootSignature();
  BuildDescriptorHeaps();
  BuildShadersAndInputLayout();
  BuildShapeGeometry();
  BuildMaterials();
  BuildRenderItems();
  BuildFrameResources();
  BuildPSOs();
  
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

  CD3DX12_ROOT_PARAMETER rootParameters[6];
  // Order from most frequent to least frequent.
  rootParameters[0].InitAsConstantBufferView(0);      // cbPerObject
  rootParameters[1].InitAsConstantBufferView(1);      // cbPass
  rootParameters[2].InitAsConstantBufferView(2);      // cbFrame
  rootParameters[3].InitAsShaderResourceView(0, 1);   // gMaterialData
  // gPortalADiffuseMap, gPortalBDiffuseMap
  rootParameters[4].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
  // gTextureMaps
  rootParameters[5].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

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
    // PhongMaterial::DiffuseSrvHeapIndex matches MaterialData::DiffuseMapIndex.
    mTextures["room"].Resource,
    mTextures["player"].Resource,
    mTextures["orange_portal"].Resource,
    mTextures["blue_portal"].Resource
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
      { nullptr, nullptr }};
  mShaders["portalBoxVS"] = d3dUtil::CompileShader(L"fx/PortalBox.hlsl", defines, "VS", "vs_5_1");
  mShaders["portalBoxPS"] = d3dUtil::CompileShader(L"fx/PortalBox.hlsl", defines, "PS", "ps_5_1");
  defines[1] = { "CLIP_PLANE", nullptr };
  mShaders["portalBoxClipPS"] = d3dUtil::CompileShader(L"fx/PortalBox.hlsl", defines, "PS", "ps_5_1");
  defines[1] = { "CLEAR_DEPTH", nullptr };
  mShaders["portalBoxClearDepthPS"] = d3dUtil::CompileShader(L"fx/PortalBox.hlsl", defines, "PS", "ps_5_1");

  defines[1] = { "DRAW_PORTAL_A", nullptr };
  defines[2] = { "DRAW_PORTAL_B", nullptr };
  mShaders["defaultVS"] = d3dUtil::CompileShader(L"fx/Default.hlsl", defines, "VS", "vs_5_1");
  mShaders["defaultPS"] = d3dUtil::CompileShader(L"fx/Default.hlsl", defines, "PS", "ps_5_1");
  defines[3] = { "CLIP_PLANE", nullptr };
  mShaders["defaultClipPS"] = d3dUtil::CompileShader(L"fx/Default.hlsl", defines, "PS", "ps_5_1");

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
  roomSubmesh.IndexCount = roomMesh.Indices.size();
  roomSubmesh.StartIndexLocation = numTotalIndices;
  roomSubmesh.BaseVertexLocation = numTotalVertices;
  numTotalIndices += roomMesh.Indices.size();
  numTotalVertices += roomMesh.Vertices.size();

  // Generate player mesh and submesh.
  GeometryGenerator::MeshData playerMesh;
  GeometryGenerator::GenerateSphere(playerMesh, 1.0f, 3);
  SubmeshGeometry playerSubmesh;
  playerSubmesh.IndexCount = playerMesh.Indices.size();
  playerSubmesh.StartIndexLocation = numTotalIndices;
  playerSubmesh.BaseVertexLocation = numTotalVertices;
  numTotalIndices += playerMesh.Indices.size();
  numTotalVertices += playerMesh.Vertices.size();

  // Generate portal-box mesh and submesh
  GeometryGenerator::MeshData portalBoxMesh;
  Portal::BuildBoxMeshData(&portalBoxMesh);
  SubmeshGeometry portalBoxSubmesh;
  portalBoxSubmesh.IndexCount = portalBoxMesh.Indices.size();
  portalBoxSubmesh.StartIndexLocation = numTotalIndices;
  portalBoxSubmesh.BaseVertexLocation = numTotalVertices;
  numTotalIndices += portalBoxMesh.Indices.size();
  numTotalVertices += portalBoxMesh.Vertices.size();

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
  roomMaterial->Specular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
  roomMaterial->MatCBIndex = 0;
  roomMaterial->DiffuseSrvHeapIndex = 0;

  PhongMaterial* playerMaterial = &mMaterials["player"];
  playerMaterial->Diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
  playerMaterial->Specular = XMFLOAT4(0.8f, 0.8f, 0.8f, 16.0f);
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

  mPlayerRenderItem.World = XMMatrixIdentity();
  mPlayerRenderItem.TexTransform = XMMatrixIdentity();
  mPlayerRenderItem.ObjCBIndex = 1;
  mPlayerRenderItem.Mat = &mMaterials["player"];
  mPlayerRenderItem.Geo = &mGeometries["shapeGeo"];
  mPlayerRenderItem.PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
  const SubmeshGeometry& playerSubmesh = mPlayerRenderItem.Geo->DrawArgs["player"];
  mPlayerRenderItem.IndexCount = playerSubmesh.IndexCount;
  mPlayerRenderItem.StartIndexLocation = playerSubmesh.StartIndexLocation;
  mPlayerRenderItem.BaseVertexLocation = playerSubmesh.BaseVertexLocation;

  mPortalBoxRenderItem.World = XMMatrixIdentity();
  mPortalBoxRenderItem.TexTransform = XMMatrixIdentity();   // unused
  mPortalBoxRenderItem.ObjCBIndex = 2;
  mPlayerRenderItem.Mat = nullptr;                          // unused
  mPortalBoxRenderItem.Geo = &mGeometries["shapeGeo"];
  mPortalBoxRenderItem.PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
  const SubmeshGeometry& portalBoxSubmesh = mPortalBoxRenderItem.Geo->DrawArgs["portalBox"];
  mPortalBoxRenderItem.IndexCount = portalBoxSubmesh.IndexCount;
  mPortalBoxRenderItem.StartIndexLocation = portalBoxSubmesh.StartIndexLocation;
  mPortalBoxRenderItem.BaseVertexLocation = portalBoxSubmesh.BaseVertexLocation;
}

void PortalsApp::BuildFrameResources() {
  for (int i = 0; i < gNumFrameResources; ++i) {
    mFrameResources.push_back(std::make_unique<FrameResource>(
        md3dDevice.Get(), /*passCount=*/1 + 2 * PORTAL_ITERATIONS,
        /*objectCount=*/2, /*materialCount=*/mMaterials.size()));
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

  // default PSO with pixels clipped against a plane and stencil test pass when equal to ref value.
  shader = mShaders["defaultClipPS"].Get();
  psoDesc.PS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.StencilEnable = true;
  psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
  ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
      &psoDesc, IID_PPV_ARGS(&mPSOs["defaultClipStencilEqual"])));


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
  shader = mShaders["portalBoxClearDepthPS"].Get();
  psoDesc.PS = { reinterpret_cast<BYTE*>(shader->GetBufferPointer()), shader->GetBufferSize() };
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  psoDesc.DepthStencilState.StencilEnable = true;
  psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
  ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
      &psoDesc, IID_PPV_ARGS(&mPSOs["portalBoxStencilEqualClearDepth"])));

  // portalbox PSO with pixels clipped against a plane, stencil test pass when equal to ref value,
  // and increments stencil values.
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

void PortalsApp::Update(const GameTimer& gt) {
  // Portals cannot be changed if either portal intersects the player or the spectator camera
  bool modifyPortal = (!mPlayerIntersectOrangePortal && !mPlayerIntersectBluePortal) &&
    !mOrangePortal.DiscIntersectSphere(
        mLeftCamera.GetPosition(), mLeftCamera.GetBoundingSphereRadius() + 0.001f) &&
    !mBluePortal.DiscIntersectSphere(
        mLeftCamera.GetPosition(), mLeftCamera.GetBoundingSphereRadius() + 0.001f);

  OnKeyboardInput(gt.DeltaTime(), modifyPortal);
  UpdateObjectCBs();
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

void PortalsApp::OnKeyboardInput(float dt, bool modifyPortal) {
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

void PortalsApp::UpdateObjectCBs() {
  UploadBuffer<ObjectConstants>* currObjectCB = mCurrentFrameResource->ObjectCB.get();
  for (RenderItem* item : { &mRoomRenderItem, &mPlayerRenderItem, &mPortalBoxRenderItem }) {
    // Only update the buffer data if the constants have changed. This needs to be tracked per
    // frame resource.
    if (item->NumFramesDirty > 0) {
      ObjectConstants objConstants;
      XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(item->World));
      XMStoreFloat4x4(&objConstants.WorldInvTranspose, XMMatrixTranspose(
          MathHelper::InverseTranspose(item->World)));
      XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(item->TexTransform));
      objConstants.MaterialIndex = item->Mat->MatCBIndex;

      currObjectCB->CopyData(item->ObjCBIndex, objConstants);

      item->NumFramesDirty--;
    }
  }
}

void PortalsApp::UpdateMaterialBuffer() {
  UploadBuffer<MaterialData>* currMaterialBuffer = mCurrentFrameResource->MaterialBuffer.get();
  for (std::pair<const std::string, PhongMaterial>& e : mMaterials) {
    // Only update the cbuffer data if the constants have changed.  If the cbuffer
    // data changes, it needs to be updated for each FrameResource.
    PhongMaterial* mat = &e.second;
    if (mat->NumFramesDirty > 0) {
      MaterialData matData;
      matData.Diffuse = mat->Diffuse;
      matData.Specular = mat->Specular;
      matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;

      currMaterialBuffer->CopyData(mat->MatCBIndex, matData);

      mat->NumFramesDirty--;
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