#version 330 core

in vec3 worldPosition;
in vec3 worldNormal;

uniform vec3 lightDirection;
uniform vec3 cameraPosition;
uniform vec3 surfaceColor;
uniform float surfaceOpacity;
uniform float specularStrength;
uniform float causticStrength;
uniform sampler2D causticsMap;
uniform vec2 causticBoundsMin;
uniform vec2 causticBoundsMax;

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

    if (causticStrength > 0.0) {
        vec2 uv = (worldPosition.xz - causticBoundsMin)
            / (causticBoundsMax - causticBoundsMin);
        if (all(greaterThanEqual(uv, vec2(0.0))) && all(lessThanEqual(uv, vec2(1.0)))) {
            float caustics = texture(causticsMap, uv).r;
            color += vec3(0.12, 0.58, 0.98) * caustics * causticStrength;
        }
    }

    fragmentColor = vec4(color, surfaceOpacity);
}
