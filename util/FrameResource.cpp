#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT clipPlanesCount, UINT passCount,
    UINT objectCount, UINT materialCount)
  : ObjectCB(device, objectCount, true),
  PassCB(device, passCount, true),
  ClipPlaneCB(device, clipPlanesCount, true),
  FrameCB(device, 1, true),
  MaterialBuffer(device, materialCount, false)
{
  ThrowIfFailed(device->CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));
}

FrameResource::~FrameResource()
{

}