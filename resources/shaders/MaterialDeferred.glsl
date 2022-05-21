
struct materialInfo
{
    float ior;
    float PerceptualRoughness;      // roughness value, as authored by the model creator (input to shader)
    vec3 f0;                        // full Reflectance color (n incidence angle)

    float AlphaRoughness;           // roughness mapped to a more linear change in the roughness (proposed by [2])
    vec3 c_diff;

    vec3 F90;                       // Reflectance color at grazing angle
    float Metallic;

    vec3 BaseColor;

    // float sheenRoughnessFactor;
    // vec3 sheenColorFactor;

    vec3 ClearcoatF0;
    vec3 ClearcoatF90;
    float ClearcoatFactor;
    vec3 ClearcoatNormal;
    float ClearcoatRoughness;

    // KHR_materials_specular 
    float SpecularWeight; // product of specularFactor and specularTexture.a

    // float transmissionFactor;

    // float thickness;
    // vec3 attenuationColor;
    // float attenuationDistance;

    // KHR_materials_iridescence
    // float iridescenceFactor;
    // float iridescenceIOR;
    // float iridescenceThickness;
};

materialInfo GetMetallicRoughnessInfo(materialInfo info, float Roughness, float Metallic)
{
    info.Metallic = Metallic;
    info.PerceptualRoughness = Roughness;
    // Achromatic f0 based on IOR.
    info.c_diff = mix(info.BaseColor.rgb,  vec3(0), info.Metallic);
    info.f0 = mix(info.f0, info.BaseColor.rgb, info.Metallic);


    info.PerceptualRoughness = Clamp01(info.PerceptualRoughness);
    info.Metallic = Clamp01(info.Metallic);

    info.AlphaRoughness = info.PerceptualRoughness * info.PerceptualRoughness;


    return info;
}




// vec3 getClearcoatNormal(normalInfo normalInfo)
// {
//     //TODO(JAcques): Clearcoat normal map
//     return normalInfo.ng;
// }

// materialInfo GetClearCoatInfo(materialInfo info, normalInfo normalInfo)
// {
//     info.ClearcoatFactor = MaterialUBO.ClearcoatFactor;
//     info.ClearcoatRoughness = MaterialUBO.ClearcoatRoughness;
//     info.ClearcoatF0 = vec3(pow((info.ior - 1.0) / (info.ior + 1.0), 2.0));
//     info.ClearcoatF90 = vec3(1.0);

//     //TODO(Jacques): Clearcoat map and roughness map

//     info.ClearcoatNormal = getClearcoatNormal(normalInfo);
//     info.ClearcoatRoughness = clamp(info.ClearcoatRoughness, 0.0, 1.0);
//     return info;
// }
