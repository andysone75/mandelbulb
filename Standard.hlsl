cbuffer cbPerObject : register(b0)
{
    matrix gWorld;
    matrix gWorldView;
    matrix gInvWorldView;
    matrix gWorldViewProj;

    float3 gCamPos;
    float gAspectRatio;

    float gPower;

    float3 gColor;
    float gDarkness;
};

struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
};

struct Ray
{
    float3 origin;
    float3 direction;
};

Ray CreateRay(float3 origin, float3 direction)
{
    Ray ray;
    ray.origin = origin;
    ray.direction = direction;
    return ray;
}

#define PI 3.14159265
#define MAX_STEPS 150.0
#define MAX_DIST 100.0
#define EPS 1e-3

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    float4 pos = float4(vin.PosL, 1.0f);

    pos = mul(pos, gWorldView);
    pos.z = 2;
    pos.x = vin.PosL.x * tan((.25 * PI) / 2 * gAspectRatio) * 2;
    pos.y = vin.PosL.y * tan((.25 * PI) / 2) * 2;
    vin.PosL = mul(pos, gInvWorldView).xyz;

    vout.PosH = mul(float4(vin.PosL, 1), gWorldViewProj);
    vout.WorldPos = mul(float4(vin.PosL, 1.0f), gWorld).xyz;

    return vout;
}

float2 SceneInfo(float3 position)
{
    float3 z = position;
    float dr = 1.0;
    float r = 0.0;
    int iterations = 0;

    for (int i = 0; i < 15; i++)
    {
        ++iterations;
        r = length(z);

        if (r > 2)
            break;

		// convert to polar coordinates
        float theta = acos(z.z / r);
        float phi = atan2(z.y, z.x);
        dr = pow(r, gPower - 1.0) * gPower * dr + 1.0;

		// scale and rotate the point
        float zr = pow(r, gPower);
        theta = theta * gPower;
        phi = phi * gPower;

		// convert back to cartesian coordinates
        z = zr * float3(sin(theta) * cos(phi), sin(phi) * sin(theta), cos(theta));
        z += position;
    }

	// Number of iterations, Calculated distance
    return float2(iterations, 0.5 * log(r) * r / dr);
}

float trace(float3 from, float3 direction) {
    float totalDistance = 0.0;
    int steps;
    for (steps = 0; steps < MAX_STEPS; steps++) {
        float3 p = from + totalDistance * direction;
        float dist = DistanceEstimator(p);
        totalDistance += dist;
        if (dist < EPS) break;
    }
    return float(steps) / float(MAX_STEPS);
}

float4 PS(VertexOut pin) : SV_Target {
    Ray ray = CreateRay(gCamPos, normalize(pin.WorldPos - gCamPos));
    float dist = trace(ray.origin, ray.direction);
    return float4(dist.xxx, 1.0);
}

// float4 PS(VertexOut pin) : SV_Target
// {
// 	// ray origin, ray direction
//     Ray ray = CreateRay(gCamPos, normalize(pin.WorldPos - gCamPos));
//     float rayDst = 0;
//     float marchSteps = 0;

//     float4 result = float4(0, 0, 0, 1);

//     while (rayDst < MAX_DIST && marchSteps < MAX_STEPS)
//     {
//         ++marchSteps;
//         float2 sceneInfo = SceneInfo(ray.origin);
//         float dist = sceneInfo.y;

// 		// Ray has hit a surface
//         if (dist <= EPS)
//         {
//             float escapeIterations = sceneInfo.x;

//             float colourB = saturate(escapeIterations / 15.0);
//             float3 colourMix = saturate(colourB * gColor);

//             result = float4(colourMix, 1.);
//             break;
//         }

//         ray.origin += ray.direction * dist;
//         rayDst += dist;
//     }

//     float rim = marchSteps / gDarkness;
//     result = saturate(result * rim + rim * float4(gColor, 1));

//     return result;
// }
