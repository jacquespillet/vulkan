struct vertex
{
  vec3 pos;
  vec3 normal;
  vec2 uv;
  vec4 tangent;
  vec3 bitangent;
  int materialIndex;
};

struct triangle {
	vertex vertices[3];
	vec3 normal;
	vec4 tangent;
	vec4 bitangent;
	vec2 uv;
	int materialIndex;
};