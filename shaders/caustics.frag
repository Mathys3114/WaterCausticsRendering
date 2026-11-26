#version 330 core

uniform float pointSize;
uniform float texelWorldArea;

in float photonFlux;

out float causticEnergy;

void main()
{
    const float sigma = 0.16;
    const float twoPi = 6.2831853;
    vec2 offset = gl_PointCoord - vec2(0.5);
    float gaussianWeight = exp(-dot(offset, offset) / (2.0 * sigma * sigma));
    float normalizedWeight = gaussianWeight
        / (twoPi * sigma * sigma * pointSize * pointSize);

    causticEnergy = photonFlux * normalizedWeight / texelWorldArea;
}
