#version 330 core
in vec4 vertex;
in vec3 normal;
in vec2 uv0;
uniform mat4 worldViewProj;
uniform mat4 world;
uniform float time;
uniform float twist_amount;
out vec2 oUv0;
out vec3 oNormal;
void main() {
    float angle = vertex.y * twist_amount + time;
    float c = cos(angle), s = sin(angle);
    vec3 pos = vec3(vertex.x * c - vertex.z * s, vertex.y, vertex.x * s + vertex.z * c);
    gl_Position = worldViewProj * vec4(pos, 1.0);
    oUv0 = uv0;
    oNormal = mat3(world) * vec3(normal.x * c - normal.z * s, normal.y, normal.x * s + normal.z * c);
}
