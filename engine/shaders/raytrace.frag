#version 330 core
in vec2 vUv;
uniform float uAspect;
out vec4 FragColor;

bool hitSphere(vec3 ro, vec3 rd, vec3 center, float radius, out float t, out vec3 normal) {
    vec3 oc = ro - center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float h = b * b - c;

    if (h < 0.0) return false;

    h = sqrt(h);
    t = -b - h;

    if (t < 0.001) t = -b + h;
    if (t < 0.001) return false;

    vec3 p = ro + rd * t;
    normal = normalize(p - center);
    return true;
}

vec3 skyColor(vec3 rd) {
    float a = 0.5 * (rd.y + 1.0);
    return mix(vec3(0.08, 0.06, 0.16), vec3(0.55, 0.75, 1.0), a);
}

void main() {
    vec2 uv = vUv * 2.0 - 1.0;
    uv.x *= uAspect;

    vec3 ro = vec3(0.0, 1.0, 4.0);
    vec3 rd = normalize(vec3(uv.x, uv.y, -1.5));

    vec3 color = skyColor(rd);

    float t;
    vec3 n;

    if (hitSphere(ro, rd, vec3(0.0, 1.0, 0.0), 1.0, t, n)) {
        vec3 reflected = reflect(rd, n);

        vec3 lightDir = normalize(vec3(-0.4, 0.8, 0.3));
        float diffuse = max(dot(n, lightDir), 0.0);

        vec3 reflectionColor = skyColor(reflected);
        vec3 baseColor = vec3(0.85, 0.85, 0.9) * diffuse;

        color = mix(baseColor, reflectionColor, 0.95);
    }

    FragColor = vec4(color, 1.0);
}
