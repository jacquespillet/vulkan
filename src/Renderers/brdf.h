#pragma once

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

struct rayPayload;


void ONB(glm::vec3 N, glm::vec3& T, glm::vec3& B);

glm::vec3 SampleHemisphere(glm::vec2 u);


glm::vec3 BaseColorToSpecularF0(glm::vec3 BaseColor, float Metalness);

glm::vec3 BaseColorToDiffuseReflectance(glm::vec3 BaseColor, float Metalness);

glm::vec3 EvalFresnelSchlick(glm::vec3 F0, float F90, float NdotS);

float Luminance(glm::vec3 RGB);
float ShadowedF90(glm::vec3 F0);

glm::vec3 SampleSpecularHalfVector(glm::vec3 Ve, glm::vec2 Alpha2D, glm::vec2 u);

// Smith G1 term (masking function) further optimized for GGX distribution (by substituting G_a into G1_GGX)
float SmithG1GGX(float Alpha, float NdotS, float AlphaSquared, float NdotSSquared);
float DistributionGGX_GeometrySmith(float Alpha, float AlphaSquared, float NdotL, float NdotV);
glm::vec3 sampleSpecularMicrofacet(glm::vec3 Vlocal, float Alpha, float AlphaSquared, glm::vec3 SpecularF0, glm::vec2 u, glm::vec3& Weight);

// This is an entry point for evaluation of all other BRDFs based on selected configuration (for indirect light)
bool SampleSpecularBRDF(glm::vec2 u, glm::vec3 N, glm::vec3 V,  glm::vec3& RayDirection,  glm::vec3& SampleWeight, glm::vec3 Color, float Roughness, float Metallic);
bool SampleDiffuseBRDF(glm::vec2 u, glm::vec3 N, glm::vec3 V,  glm::vec3& RayDirection,  glm::vec3& SampleWeight, glm::vec3 Color, float Metallic);
float GetBrdfProbability(rayPayload RayPayload, glm::vec3 V, glm::vec3 ShadingNormal, glm::vec3 Color, float Metallic);


// This is an entry point for evaluation of all other BRDFs based on selected configuration (for direct light)
glm::vec3 EvalCombinedBRDF(glm::vec3 N, glm::vec3 L, glm::vec3 V, glm::vec3 Color, float Metallic);