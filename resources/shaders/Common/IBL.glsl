
vec3 GetDiffuseLight(vec3 Direction)
{
    if(SceneUbo.Data.BackgroundType == 0)
    {
        Direction.y *=-1;
        return texture(IrradianceMap, Direction).rgb;
    }
    else
    {
        return SceneUbo.Data.BackgroundColor * SceneUbo.Data.BackgroundIntensity;
    }
}


// SpecularWeight is introduced with KHR_materials_specular
vec3 GetIBLRadianceLambertian(vec3 Normal, vec3 View, float Roughness, vec3 DiffuseColor, vec3 F0, float SpecularWeight)
{
    float NdotV = ClampedDot(Normal, View);
    //Samples the brdf lut table. x is angle, y is roughness
    vec2 brdfSamplePoint = clamp(vec2(NdotV, Roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 brdfSample = texture(BRDFLUT, brdfSamplePoint).rg;

    //Get irradiance for current normal
    vec3 irradiance = GetDiffuseLight(Normal);
    
    // https://bruop.github.io/ibl/#single_scattering_results Single Scattering
    vec3 Fr = max(vec3(1.0 - Roughness), F0) - F0;
    vec3 k_S = F0 + Fr * pow(1.0 - NdotV, 5.0);
    vec3 FssEss = SpecularWeight * k_S * brdfSample.x + brdfSample.y; // <--- GGX / specular light contribution (scale it down if the SpecularWeight is low)

    // Multiple scattering, from Fdez-Aguera
    float Ems = (1.0 - (brdfSample.x + brdfSample.y));
    vec3 F_avg = SpecularWeight * (F0 + (1.0 - F0) / 21.0);
    vec3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);
    vec3 k_D = DiffuseColor * (1.0 - FssEss + FmsEms); // we use +FmsEms as indicated by the formula in the blog post (might be a typo in the implementation)

    return (FmsEms + k_D) * irradiance;
}


vec4 GetSpecularSample(vec3 Reflection, float LOD)
{
    if(SceneUbo.Data.BackgroundType == 0)
    {
        //TODO(Jacques): Rotation
        Reflection.y *= -1;
        return textureLod(PrefilteredEnv, Reflection, LOD);
    }
    else
    {
        return vec4(SceneUbo.Data.BackgroundColor, 1) * SceneUbo.Data.BackgroundIntensity;
    }
}



vec3 GetIBLRadianceGGX(vec3 Normal, vec3 View, float Roughness, vec3 F0, float SpecularWeight)
{
    float NdotV = ClampedDot(Normal, View);
    
    //TODO(Jacques): Max mip 
    float LOD = Roughness * float(5 - 1);
    vec3 reflection = normalize(reflect(-View, Normal));

    vec2 brdfSamplePoint = clamp(vec2(NdotV, Roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec2 f_ab = texture(BRDFLUT, brdfSamplePoint).rg;
    vec4 specularSample = GetSpecularSample(reflection, LOD);

    vec3 specularLight = specularSample.rgb;

    // see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
    vec3 Fr = max(vec3(1.0 - Roughness), F0) - F0;
    vec3 k_S = F0 + Fr * pow(1.0 - NdotV, 5.0);
    vec3 FssEss = k_S * f_ab.x + f_ab.y;

    return SpecularWeight * specularLight * FssEss;
}
