#pragma once
#include <DirectXMath.h>
#include "../Common/MathHelper.h"
using namespace DirectX;
// Calss stores constant buffers - data, which is same for all of the verticies in a drawcall / material

class objectConstf4x4
{
	//Analog of MVP from OpenGL - multiplication of vertext position on this matrix will transfer it into camera view
public:
	XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

