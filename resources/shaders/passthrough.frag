// passthrough.frag — BBFx v2.8 basic fragment shader
// Diffuse + ambient lighting

#version 330 core

uniform vec4 lightDiffuse;
uniform vec4 ambientLight;
uniform vec4 materialDiffuse;

in vec3 vNormal;
in vec2 vTexCoord;
in vec3 vWorldPos;

out vec4 fragColor;

void main() {
    vec3 n = normalize(vNormal);
    // Simple directional light from above-right
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(n, lightDir), 0.0);

    vec3 ambient = ambientLight.rgb * 0.3;
    vec3 diffuse = materialDiffuse.rgb * diff * 0.7;

    fragColor = vec4(ambient + diffuse, 1.0);
}
