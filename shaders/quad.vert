#version 330 core

layout (location = 0) in vec3 position;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform mat3 normalMatrix;

out vec3 worldPosition;
out vec3 worldNormal;

void main()
{
    const float amplitude = 0.18;
    const float wavelength = 2.5;
    const float speed = 0.8;
    const vec2 waveDirection = vec2(0.8, 0.6);
    const float waveNumber = 6.2831853 / wavelength;

    float phase = waveNumber * dot(waveDirection, position.xz) - waveNumber * speed * time;
    vec3 displacedPosition = position;
    displacedPosition.y += amplitude * sin(phase);

    float slopeX = amplitude * waveNumber * waveDirection.x * cos(phase);
    float slopeZ = amplitude * waveNumber * waveDirection.y * cos(phase);
    vec3 objectNormal = normalize(vec3(-slopeX, 1.0, -slopeZ));

    vec4 worldPositionHomogeneous = model * vec4(displacedPosition, 1.0);
    worldPosition = worldPositionHomogeneous.xyz;
    worldNormal = normalize(normalMatrix * objectNormal);
    gl_Position = projection * view * worldPositionHomogeneous;
}
