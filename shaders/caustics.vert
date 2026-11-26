#version 330 core

layout (location = 0) in vec3 position;

uniform mat4 model;
uniform mat3 normalMatrix;
uniform float time;
uniform float waveAmplitude;
uniform vec3 lightDirection;
uniform float refractiveIndexRatio;
uniform float receiverHeight;
uniform vec2 causticBoundsMin;
uniform vec2 causticBoundsMax;
uniform float surfaceCellArea;
uniform float pointSize;
uniform float lightIntensity;

out float photonFlux;

#include "wave.glsl"

void main()
{
    WaveSample wave = evaluateWave(position.xz, time, waveAmplitude);
    vec3 displacedPosition = position;
    displacedPosition.y += wave.height;

    vec3 objectNormal = normalize(vec3(
        -wave.heightGradient.x,
        1.0,
        -wave.heightGradient.y
    ));

    vec4 worldPositionHomogeneous = model * vec4(displacedPosition, 1.0);
    vec3 worldPosition = worldPositionHomogeneous.xyz;
    vec3 worldNormal = normalize(normalMatrix * objectNormal);
    vec3 worldTangentX = mat3(model) * vec3(1.0, wave.heightGradient.x, 0.0);
    vec3 worldTangentZ = mat3(model) * vec3(0.0, wave.heightGradient.y, 1.0);

    vec3 light = normalize(lightDirection);
    float incidentCosine = max(dot(worldNormal, light), 0.0);
    vec3 incident = -light;
    vec3 transmitted = refract(incident, worldNormal, refractiveIndexRatio);

    if (incidentCosine <= 0.0 || transmitted.y >= -0.0001) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        gl_PointSize = 0.0;
        photonFlux = 0.0;
        return;
    }

    float distanceToReceiver = (receiverHeight - worldPosition.y) / transmitted.y;
    if (distanceToReceiver <= 0.0) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        gl_PointSize = 0.0;
        photonFlux = 0.0;
        return;
    }

    vec3 receiverPosition = worldPosition + distanceToReceiver * transmitted;
    vec2 uv = (receiverPosition.xz - causticBoundsMin) / (causticBoundsMax - causticBoundsMin);
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        gl_PointSize = 0.0;
        photonFlux = 0.0;
        return;
    }

    float surfaceAreaScale = length(cross(worldTangentX, worldTangentZ));
    photonFlux = lightIntensity * incidentCosine * surfaceCellArea * surfaceAreaScale;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    gl_PointSize = pointSize;
}
