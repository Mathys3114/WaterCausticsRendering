#version 330 core

layout (location = 0) in vec3 position;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform mat3 normalMatrix;
uniform float waveAmplitude;

out vec3 worldPosition;
out vec3 worldNormal;

#include "wave.glsl"

void main()
{
    WaveSample wave = evaluateWave(position.xz, time, waveAmplitude);
    vec3 displacedPosition = position;
    displacedPosition.y += wave.height;

    vec3 objectNormal = normalize(vec3(-wave.heightGradient.x, 1.0, -wave.heightGradient.y));

    vec4 worldPositionHomogeneous = model * vec4(displacedPosition, 1.0);
    worldPosition = worldPositionHomogeneous.xyz;
    worldNormal = normalize(normalMatrix * objectNormal);
    gl_Position = projection * view * worldPositionHomogeneous;
}
