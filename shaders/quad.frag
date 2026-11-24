#version 330 core

in vec3 worldPosition;
in vec3 worldNormal;

uniform vec3 lightDirection;
uniform vec3 cameraPosition;
uniform vec3 surfaceColor;
uniform float surfaceOpacity;
uniform float specularStrength;
uniform float causticStrength;

out vec4 fragmentColor;

float hash12(vec2 point)
{
    vec3 value = fract(vec3(point.xyx) * 0.1031);
    value += dot(value, value.yzx + 33.33);
    return fract((value.x + value.y) * value.z);
}

vec2 hash22(vec2 point)
{
    return vec2(hash12(point), hash12(point + vec2(19.19, 73.73)));
}

float causticPattern(vec2 position)
{
    const float cellDensity = 1.6;
    vec2 gridPosition = position * cellDensity;
    vec2 cell = floor(gridPosition);
    vec2 localPosition = fract(gridPosition);
    float nearestDistance = 10.0;
    float secondNearestDistance = 10.0;

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 neighbor = vec2(float(x), float(y));
            vec2 site = neighbor + 0.2 + 0.6 * hash22(cell + neighbor);
            float distanceToSite = length(site - localPosition);

            if (distanceToSite < nearestDistance) {
                secondNearestDistance = nearestDistance;
                nearestDistance = distanceToSite;
            } else if (distanceToSite < secondNearestDistance) {
                secondNearestDistance = distanceToSite;
            }
        }
    }

    float edgeDistance = secondNearestDistance - nearestDistance;
    return 1.0 - smoothstep(0.025, 0.12, edgeDistance);
}

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
        float footprint = 1.0 - smoothstep(
            2.3,
            2.5,
            max(abs(worldPosition.x), abs(worldPosition.z))
        );
        float caustics = causticPattern(worldPosition.xz) * footprint;
        color += vec3(0.12, 0.58, 0.78) * caustics * causticStrength;
    }

    fragmentColor = vec4(color, surfaceOpacity);
}
