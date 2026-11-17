#version 330 core

in vec3 worldPosition;
in vec3 worldNormal;

uniform vec3 lightDirection;
uniform vec3 cameraPosition;
uniform vec3 surfaceColor;
uniform float surfaceOpacity;
uniform float specularStrength;

out vec4 fragmentColor;

void main()
{
    const vec3 specularColor = vec3(0.75, 0.9, 1.0);

    vec3 normal = normalize(worldNormal);
    vec3 light = normalize(lightDirection);
    vec3 viewDirection = normalize(cameraPosition - worldPosition);
    vec3 halfwayDirection = normalize(light + viewDirection);

    float diffuse = max(dot(normal, light), 0.0);
    float specular = pow(max(dot(normal, halfwayDirection), 0.0), 64.0);
    vec3 color = surfaceColor * (0.2 + 0.8 * diffuse)
        + specularColor * (specularStrength * specular);

    fragmentColor = vec4(color, surfaceOpacity);
}
