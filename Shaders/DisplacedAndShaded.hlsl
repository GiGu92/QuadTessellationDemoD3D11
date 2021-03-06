//--------------------------------------------------------------------------------------
// File: DisplacedAndShaded.hlsl
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------
Texture2D texDiffuse : register(t[0]);
Texture2D texDisplacement : register(t[1]);
Texture2D texNormal : register(t[2]);

//--------------------------------------------------------------------------------------
// Samplers
//--------------------------------------------------------------------------------------
SamplerState samPoint : register(s[0]);
SamplerState samLinear : register(s[1]);


//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
cbuffer ConstantBuffer : register(b0)
{
	matrix World;
	matrix View;
	matrix Projection;
	float4 Pos;
	float4 LightPos;
	float4 LightColor;
	float4 Eye;
	float TessellationFactor;
	float Scaling;
	float DisplacementLevel;
}


//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct VS_CP_INPUT
{
	float3 PosOS : POSITION;
	float3 NormOS : NORMAL;
	float2 TexCoord : TEXCOORD0;
};

struct VS_CP_OUTPUT
{
	float3 PosWS : POSITION;
	float3 NormWS : NORMAL;
	float2 TexCoord : TEXCOORD0;
};

struct HS_CONST_DATA_OUTPUT
{
	float Edges[3] : SV_TessFactor;
	float Inside[1] : SV_InsideTessFactor;
};

struct HS_CP_OUTPUT
{
	float3 PosWS : WORLDPOS;
	float2 TexCoord : TEXCOORD0;
	float3 NormWS : NORMAL;
};

struct DS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	float3 LightWS : LIGHTVECTORTS;
	float3 ViewWS : VIEWVECTORS;
	float3 NormWS : NORMAL;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_CP_OUTPUT VS(VS_CP_INPUT input)
{
	VS_CP_OUTPUT output;
	
	// Compute position and normal in world space
	float3 PosWS = mul(input.PosOS, (float3x3) World);
	float3 NormalWS = mul(input.NormOS, (float3x3) World);
	NormalWS = normalize(NormalWS);

	// Output position and normal
	output.PosWS = PosWS.xyz;
	output.NormWS = NormalWS;
	output.TexCoord = input.TexCoord;

	return output;
}


//--------------------------------------------------------------------------------------
// Hull Shader constant function
//--------------------------------------------------------------------------------------
HS_CONST_DATA_OUTPUT ConstHS(InputPatch<VS_CP_OUTPUT, 3> ip, uint PatchID : SV_PrimitiveID)
{
	HS_CONST_DATA_OUTPUT output;

	// Calculating distance adaptive tessellation factor
	/*float maxTessellationDistance = 40;
	float minTessellationDistance = 10;
	float distance = length(Eye - Pos) - minTessellationDistance;
	if (distance < 0) distance = 0;
	float t = distance / maxTessellationDistance;
	if (t > 1) t = 1;
	float adaptiveTessFactor = (1 - t) * TessellationFactor + t;*/

	// Setting calculated tessellation factor globally for all tessellation factors
	/*output.Edges[0] = output.Edges[1] = output.Edges[2] = output.Edges[3] = adaptiveTessFactor;
	output.Inside[0] = output.Inside[1] = adaptiveTessFactor;*/

	output.Edges[0] = output.Edges[1] = output.Edges[2] = TessellationFactor;
	output.Inside[0] = TessellationFactor;


	return output;
}


//--------------------------------------------------------------------------------------
// Hull Shader
//--------------------------------------------------------------------------------------
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstHS")]
HS_CP_OUTPUT HS(InputPatch<VS_CP_OUTPUT, 3> p,
	uint i : SV_OutputControlPointID,
	uint PatchID : SV_PrimitiveID)
{
	HS_CP_OUTPUT output;

	// Pass through the position and the normal vectors
	output.PosWS = p[i].PosWS;
	output.NormWS = p[i].NormWS;
	output.TexCoord = p[i].TexCoord;

	return output;
}


//--------------------------------------------------------------------------------------
// Domain Shader
//--------------------------------------------------------------------------------------
[domain("tri")]
DS_OUTPUT DS(HS_CONST_DATA_OUTPUT input,
	float3 BaryCoords : SV_DomainLocation,
	const OutputPatch<HS_CP_OUTPUT, 3> TriPatch)
{
	DS_OUTPUT output;

	// Interpolating position
	float3 vWorldPos = BaryCoords.x * TriPatch[0].PosWS +
						BaryCoords.y * TriPatch[1].PosWS +
						BaryCoords.z * TriPatch[2].PosWS;

	// Interpolating normal vector
	float3 vNormal = BaryCoords.x * TriPatch[0].NormWS +
					 BaryCoords.y * TriPatch[1].NormWS +
					 BaryCoords.z * TriPatch[2].NormWS;
	vNormal = normalize(vNormal);
	output.NormWS = vNormal;

	// Interpolating and outputting texture coordinates
	output.TexCoord = BaryCoords.x * TriPatch[0].TexCoord +
					  BaryCoords.y * TriPatch[1].TexCoord +
					  BaryCoords.z * TriPatch[2].TexCoord;

	// Displacing generated vertexes
	float4 texSample = texDisplacement.SampleLevel(samPoint, output.TexCoord, 0);
	vWorldPos += /*vNormal * */ float3(0,1,0) * texSample.r * Scaling * DisplacementLevel;
	output.Pos = mul(float4(vWorldPos, 1), mul(View, Projection));

	// Calculating light vector
	output.LightWS = LightPos - vWorldPos;

	// Calculating view vector
	output.ViewWS = Eye - vWorldPos;	

	return output;
}

//--------------------------------------------------------------------------------------
// Function:    ComputeIllumination
// 
// Description: Computes phong illumination for the given pixel using its attribute 
//              textures and a light vector.
//--------------------------------------------------------------------------------------
float4 ComputeIllumination(float2 texCoord, float3 vLightTS, float3 vViewTS)
{
	// Sample the normal from the normal map for the given texture sample:
	float3 vNormalTS = normalize(texNormal.Sample(samLinear, texCoord) * 2.0 - 1.0);

	// Setting base color
	float4 cBaseColor = texDiffuse.Sample(samLinear, texCoord);

	// Compute ambient component
	float ambientPower = 0.1f;
	float4 ambientColor = float4(1, 1, 1, 1);
	float4 cAmbient = ambientColor * ambientPower;

	// Compute diffuse color component:
	float4 cDiffuse = saturate(dot(vNormalTS, vLightTS));

	// Compute the specular component if desired:  
	float3 R = normalize (2 * dot(vLightTS, vNormalTS) * vNormalTS - vLightTS);
	float shininess = 20;
	float4 specularColor = float4(1, 1, 1, 1);
	float4 cSpecular = pow(saturate(dot(R, vViewTS)), shininess) * specularColor;
		
	// Composite the final color:
	float4 cFinalColor = (cAmbient + cDiffuse) * cBaseColor + cSpecular;

	return cFinalColor;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(DS_OUTPUT input) : SV_Target
{
	float3 LightTS = normalize(input.LightWS);
	float3 ViewTS = normalize(input.ViewWS);

	//float4 finalColor = ComputeIllumination(input.TexCoord, LightTS, ViewTS);
	float4 finalColor = float4(input.TexCoord, 0, 1);

	return finalColor;
}

//--------------------------------------------------------------------------------------
// Solid Pixel Shader for Wireframe
//--------------------------------------------------------------------------------------
float4 SolidPS(DS_OUTPUT input) : SV_Target
{
	return float4 (1, 1, 1, 1);
}