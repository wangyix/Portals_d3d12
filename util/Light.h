#ifndef LIGHT_H
#define LIGHT_H

#include <DirectXMath.h>

using namespace DirectX;

struct DirectionalLight
{
	// struct constructor: zeros everything
	DirectionalLight() { ZeroMemory(this, sizeof(this)); }

	XMFLOAT4 Ambient;
	XMFLOAT4 Diffuse;
	XMFLOAT4 Specular;
	XMFLOAT3 Direction;
	float pad;			// allows arrays of lights
};

struct PhongMaterial
{
  PhongMaterial() { ZeroMemory(this, sizeof(this)); }

	XMFLOAT4 Ambient;
	XMFLOAT4 Diffuse;
	XMFLOAT4 Specular;	// cos exponent in w component
	XMFLOAT4 Reflect;
};

#endif