struct Material {
	vec4 baseColor;
	int baseColorTextureIndex;
	int normalTextureIndex;
	int emissionTextureIndex;
	int occlusionTextureIndex;
	int metallicRoughnessTextureIndex;
	uint type;
};