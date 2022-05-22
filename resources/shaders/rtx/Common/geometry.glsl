Triangle unpackTriangle(uint index, int vertexSize) {
	Triangle tri;
	const uint triIndex = index * 3;

	ObjBuffers objResource = scene_desc.i[gl_InstanceCustomIndexEXT];
	Indices    indices     = Indices(objResource.indices);
	Vertices   vertices    = Vertices(objResource.vertices);

	// Unpack vertices
	// Data is packed as vec4 so we can map to the glTF vertex structure from the host side
	for (uint i = 0; i < 3; i++) {
		const uint offset = indices.i[triIndex + i] * (vertexSize / 16);

		vec4 d0 = vertices.v[offset + 0]; // Pos.xyz, uv.x
		vec4 d1 = vertices.v[offset + 1]; // Normal.xyz, uv.y,
		vec4 d2 = vertices.v[offset + 2]; // Tangent.xyz, .
		
		tri.vertices[i].pos = d0.xyz;
		tri.vertices[i].uv = vec2(d0.w, d1.w);
		tri.vertices[i].normal = d1.xyz;
		tri.vertices[i].tangent = vec4(d2.xyz, 1);
		tri.vertices[i].materialIndex = 0;
	}

	// Calculate values at barycentric coordinates
	vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
	tri.normal = normalize(tri.vertices[0].normal * barycentricCoords.x + tri.vertices[1].normal * barycentricCoords.y + tri.vertices[2].normal * barycentricCoords.z);
	tri.tangent = tri.vertices[0].tangent * barycentricCoords.x + tri.vertices[1].tangent * barycentricCoords.y + tri.vertices[2].tangent * barycentricCoords.z;
	tri.uv = tri.vertices[0].uv * barycentricCoords.x + tri.vertices[1].uv * barycentricCoords.y + tri.vertices[2].uv * barycentricCoords.z;
	tri.materialIndex = tri.vertices[0].materialIndex;
	
	return tri;
}