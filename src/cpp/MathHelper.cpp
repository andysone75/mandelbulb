#include "MathHelper.h"
#include <float.h>
#include <cmath>

using namespace DirectX;

const float MathHelper::Infinity = FLT_MAX;
const float MathHelper::Pi = 3.1415926535f;

float MathHelper::AngleFromXY(float x, float y)
{
	float theta = .0f;

	// Quadrant I or IV
	if (x >= .0f)
	{
		// If x = 0, then atanf(y / x) = +pi/2 if y > 0
		//				  atanf(y / x) = -pi/2 if y < 0
		theta = atanf(y / x); // in [-pi/2, +pi/2]

		if (theta < .0f)
			theta += 2.0f * Pi; // in [0, 2*pi)
	}
	// Quadrant II or III
	else
		theta = atanf(y / x) + Pi; // in [0, 2*pi)

	return theta;
}

XMVECTOR MathHelper::RandUnitVec3()
{
	XMVECTOR One = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	XMVECTOR Zero = XMVectorZero();

	while (true)
	{
		XMVECTOR v = XMVectorSet(MathHelper::Rand(-1.0f, 1.0f), MathHelper::Rand(-1.0f, 1.0f), MathHelper::Rand(-1.0f, 1.0f), .0f);
		
		if (XMVector3Greater(XMVector3LengthSq(v), One))
			continue;
		
		return XMVector3Normalize(v);
	}
}

XMVECTOR MathHelper::RandHemisphereUnitVec3(XMVECTOR n)
{
	XMVECTOR One = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	XMVECTOR Zero = XMVectorZero();

	while (true)
	{
		XMVECTOR v = XMVectorSet(MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), .0f);
	
		if (XMVector3Greater(XMVector3LengthSq(v), One))
			continue;

		if (XMVector3Less(XMVector3Dot(n, v), Zero))
			continue;

		return XMVector3Normalize(v);
	}
}