triangle UnpackTriangle(uint index, int vertexSize) {
	triangle Triangle;
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
		vec4 d2 = vertices.v[offset + 2]; // Tangent.xyz, Tangent handedness .
		vec4 d3 = vertices.v[offset + 3]; // MatInx, .
		
		Triangle.vertices[i].pos = d0.xyz;
		Triangle.vertices[i].uv = vec2(d0.w, d1.w);
		Triangle.vertices[i].normal = d1.xyz;
		Triangle.vertices[i].tangent = vec4(d2.xyz, 1);
		Triangle.vertices[i].materialIndex = int(d3.x);
	}

	// Calculate values at barycentric coordinates
	vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
	Triangle.normal = normalize(Triangle.vertices[0].normal * barycentricCoords.x + Triangle.vertices[1].normal * barycentricCoords.y + Triangle.vertices[2].normal * barycentricCoords.z);
	Triangle.tangent = Triangle.vertices[0].tangent * barycentricCoords.x + Triangle.vertices[1].tangent * barycentricCoords.y + Triangle.vertices[2].tangent * barycentricCoords.z;
	Triangle.uv = Triangle.vertices[0].uv * barycentricCoords.x + Triangle.vertices[1].uv * barycentricCoords.y + Triangle.vertices[2].uv * barycentricCoords.z;
	Triangle.materialIndex = Triangle.vertices[0].materialIndex;
	
	return Triangle;
}