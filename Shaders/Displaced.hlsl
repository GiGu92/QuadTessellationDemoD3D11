//--------------------------------------------------------------------------------------
// File: Displaced.hlsl
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------
Texture2D texDisplacement : register(t0);


//--------------------------------------------------------------------------------------
// Samplers
//--------------------------------------------------------------------------------------
SamplerState samPoint : register(s0);


//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
cbuffer ConstantBuffer : register(b0)
{
	matrix World;
	matrix View;
	matrix Projection;
	float4 LightDir;
	float4 LightColor;
	float TessellationFactor;
}


//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct VS_CP_INPUT
{
	float4 Pos : POSITION;
	float3 Norm : NORMAL;
};

struct VS_CP_OUTPUT
{
	float4 Pos : POSITION;
	float3 Norm : NORMAL;
};

struct HS_CONST_DATA_OUTPUT
{
	float Edges[4] : SV_TessFactor;
	float Inside[2] : SV_InsideTessFactor;
};

struct HS_OUTPUT
{
	float3 Pos : POSITION;
	float3 Norm : NORMAL;
};

struct DS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float3 Norm : TEXCOORD0;
};

struct PS_INPUT
{
	float4 Pos : SV_POSITION;
	float3 Norm : TEXCOORD0;
};


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_CP_OUTPUT VS(VS_CP_INPUT input)
{
	VS_CP_OUTPUT output;
	
	output.Pos = mul(input.Pos, World);
	output.Norm = mul(input.Norm, World);

	return output;
}


//--------------------------------------------------------------------------------------
// Hull Shader constant function
//--------------------------------------------------------------------------------------
HS_CONST_DATA_OUTPUT ConstHS(InputPatch<VS_CP_OUTPUT, 4> ip, uint PatchID : SV_PrimitiveID)
{
	HS_CONST_DATA_OUTPUT output;

	output.Edges[0] = output.Edges[1] = output.Edges[2] = output.Edges[3] = TessellationFactor;
	output.Inside[0] = output.Inside[1] = TessellationFactor;

	return output;
}


//--------------------------------------------------------------------------------------
// Hull Shader
//--------------------------------------------------------------------------------------
[domain("quad")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("ConstHS")]
HS_OUTPUT HS(InputPatch<VS_CP_OUTPUT, 4> p,
	uint i : SV_OutputControlPointID,
	uint PatchID : SV_PrimitiveID)
{
	HS_OUTPUT output;

	output.Pos = p[i].Pos;
	output.Norm = p[i].Norm;

	return output;

}


//--------------------------------------------------------------------------------------
// Domain Shader
//--------------------------------------------------------------------------------------
[domain("quad")]
DS_OUTPUT DS(HS_CONST_DATA_OUTPUT input,
	float2 UV : SV_DomainLocation,
	const OutputPatch<HS_OUTPUT, 4> quad)
{
	DS_OUTPUT output;

	float3 zPos1 = lerp(quad[0].Pos, quad[3].Pos, UV.y);
	float3 zPos2 = lerp(quad[1].Pos, quad[2].Pos, UV.y);
	float3 xzPos = lerp(zPos1, zPos2, UV.x);

	float3 finalPos = xzPos;
	float4 texSample = texDisplacement.SampleLevel(samPoint, UV, 0);
	finalPos.y += texSample.r;
	output.Pos = mul(float4(finalPos, 1), mul(View, Projection));

	return output;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
	float4 finalColor = float4( 1, 1, 1, 1 );

	//finalColor = saturate(dot((float3)LightDir, input.Norm) * LightColor);

	finalColor.a = 1;
	return finalColor;
}