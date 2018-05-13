#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT objectCount, UINT clipPlaneCount,
    UINT lightWorldCount, UINT passCount, UINT materialCount)
  : ObjectCB(device, objectCount, true),
  ClipPlaneCB(device, clipPlaneCount, true),
  World2CB(device, lightWorldCount, true),
  PassCB(device, passCount, true),
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