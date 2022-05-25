float ClampedDot(vec3 x, vec3 y)
{
    return clamp(dot(x, y), 0.0, 1.0);
}

float Clamp01(float value)
{
    return clamp(value, 0.0, 1.0);
}


float max3(vec3 v)
{
    return max(max(v.x, v.y), v.z);
}


float sq(float t)
{
    return t * t;
}

vec2 sq(vec2 t)
{
    return t * t;
}

vec3 sq(vec3 t)
{
    return t * t;
}

vec4 sq(vec4 t)
{
    return t * t;
}


float ApplyIorToRoughness(float roughness, float ior)
{
    return roughness * clamp(ior * 2.0 - 2.0, 0.0, 1.0);
}
