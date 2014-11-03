//--------------------------------------------------------------------------------------
// File: Shaded.hlsl
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


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
}


//--------------------------------------------------------------------------------------
struct VS_INPUT
{
	float4 Pos : POSITION;
	float3 Norm : NORMAL;
};

struct PS_INPUT
{
	float4 Pos : SV_POSITION;
	float3 Norm : TEXCOORD0;
};


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
	PS_INPUT output = (PS_INPUT)0;
	output.Pos = mul(input.Pos, World);
	output.Pos = mul(output.Pos, View);
	output.Pos = mul(output.Pos, Projection);
	output.Norm = mul(input.Norm, World);

	return output;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
	float4 finalColor;

	finalColor = saturate(dot((float3)LightDir, input.Norm) * LightColor);

	finalColor.a = 1;
	return finalColor;
}