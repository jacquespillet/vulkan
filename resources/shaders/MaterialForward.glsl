
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


struct normalInfo {
    vec3 ng;   // Geometry normal
    vec3 t;    // Geometry tangent
    vec3 b;    // Geometry bitangent
    vec3 n;    // Shading normal
    vec3 ntex; // Normal from texture, scaling is accounted for.
};

// Get normal, tangent and bitangent vectors.
normalInfo getNormalInfo(vec3 v)
{
    vec3 n, t, b, ng;
   
    t = normalize(TBN[0]);
    b = normalize(TBN[1]);
    ng = normalize(TBN[2]);

    normalInfo info;
    info.ng = ng;
    info.n = ng;

    if(HAS_NORMAL_MAP>0 && MaterialUBO.UseNormalMap >0)
    {
        info.ntex = texture(samplerNormal, FragUv).rgb * 2.0 - vec3(1.0);
        info.ntex = normalize(info.ntex);
        info.n = normalize(mat3(t, b, ng) * info.ntex);
    }

    info.t = t;
    info.b = b;
    return info;
}

materialInfo GetMetallicRoughnessInfo(materialInfo info)
{
    info.Metallic = MaterialUBO.Metallic;
    info.PerceptualRoughness = MaterialUBO.Roughness;

// #ifdef HAS_METALLIC_ROUGHNESS_MAP
    if(HAS_METALLIC_ROUGHNESS_MAP>0 &&  MaterialUBO.UseMetallicRoughnessMap >0)
    {
        vec4 mrSample = texture(samplerSpecular, FragUv);
        info.PerceptualRoughness *= mrSample.g;
        info.Metallic *= mrSample.b;
    }
// #endif

    // Achromatic f0 based on IOR.
    info.c_diff = mix(info.BaseColor.rgb,  vec3(0), info.Metallic);
    info.f0 = mix(info.f0, info.BaseColor.rgb, info.Metallic);


    info.PerceptualRoughness = Clamp01(info.PerceptualRoughness);
    info.Metallic = Clamp01(info.Metallic);

    info.AlphaRoughness = info.PerceptualRoughness * info.PerceptualRoughness;


    return info;
}


vec4 GetBaseColor()
{
    vec4 BaseColor = vec4(1);

    BaseColor = vec4(MaterialUBO.BaseColor, 1);

// #ifdef HAS_BASE_COLOR_MAP
    if(HAS_BASE_COLOR_MAP>0 && MaterialUBO.UseBaseColorMap>0)
    {
        vec4 sampleCol = texture(samplerColor, FragUv); 
        BaseColor.rgb *= pow(sampleCol.rgb, vec3(2.2));
        BaseColor.a *= sampleCol.a;
    }
// #endif


    return BaseColor;
}



vec3 getClearcoatNormal(normalInfo normalInfo)
{
    //TODO(JAcques): Clearcoat normal map
    return normalInfo.ng;
}

materialInfo GetClearCoatInfo(materialInfo info, normalInfo normalInfo)
{
    info.ClearcoatFactor = MaterialUBO.ClearcoatFactor;
    info.ClearcoatRoughness = MaterialUBO.ClearcoatRoughness;
    info.ClearcoatF0 = vec3(pow((info.ior - 1.0) / (info.ior + 1.0), 2.0));
    info.ClearcoatF90 = vec3(1.0);

    //TODO(Jacques): Clearcoat map and roughness map

    info.ClearcoatNormal = getClearcoatNormal(normalInfo);
    info.ClearcoatRoughness = clamp(info.ClearcoatRoughness, 0.0, 1.0);
    return info;
}
