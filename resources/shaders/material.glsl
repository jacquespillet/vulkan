struct material {
    int BaseColorTextureID;
    int MetallicRoughnessTextureID;
    int NormalMapTextureID;
    int EmissionMapTextureID;
    
    int OcclusionMapTextureID;
    int UseBaseColorMap;
    int UseMetallicRoughnessMap;
    int UseNormalMap;

    int UseEmissionMap;
    int UseOcclusionMap;
    int AlphaMode;
    int DebugChannel;

    float Roughness;
    float AlphaCutoff;
    float ClearcoatRoughness;
    float padding1;
    
    float Metallic;
    float OcclusionStrength;
    float EmissiveStrength;
    float ClearcoatFactor;
    
    vec3 BaseColor;
    float Opacity;
    
    vec3 Emission;
    float Exposure; 
};