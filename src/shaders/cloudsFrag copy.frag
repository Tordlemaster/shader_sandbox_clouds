#version 460 core

in vec2 TexCoords;
in vec2 FragPos;

out vec4 FragColor;

uniform float invAspectRatio;
uniform sampler2D weatherMap;
uniform sampler3D shapeNoise;

uniform float testSampleHeight;

layout (std140, binding = 0) uniform camera {
    vec2 camAngle;
    vec3 camPos;
};

float remap(float val, float l0, float h0, float ln, float hn) {
    return ln + ((val - l0) * (hn - ln)) / (h0 - l0);
}

float cloudBottomShape(float ph) {
    return clamp(remap(ph, 0.0, 0.07, 0.0, 1.0), 0.0, 1.0);
}

float cloudTopShape(float ph, float wh) {
    wh = clamp(wh + 0.12, 0.0, 1.0);
    return clamp(remap(ph, wh * 0.2, wh, 1.0, 0.0), 0.0, 1.0);
}

float cloudShapeAlter(float ph, float wh) {
    return cloudBottomShape(ph) * cloudTopShape(ph, wh);
}

float cloudBottomDensity(float ph) {
    return clamp(remap(ph, 0, 0.2, 0, 1), 0.0, 1.0);
}

float cloudTopDensity(float ph) {
    return clamp(remap(ph, 0.9, 1.0, 1.0, 0.0), 0.0, 1.0);
}

float cloudDensityAlter(float ph, float gd, float wd) {
    return gd * cloudBottomDensity(ph) * cloudTopDensity(ph) * wd * 2.0;
}

float weatherMapCoverage(vec2 coords, float gc) {
    vec4 weatherMapSample = texture(weatherMap, coords / 10000.0);
    return max(weatherMapSample.r, clamp(gc - 0.5, 0.0, 1.0) * weatherMapSample.g * 2);
}

float sampleShapeNoise(vec3 coords) {
    vec4 SNsample = texture(shapeNoise, coords / 1000.0);
    return remap(SNsample.r, dot(SNsample.gba, vec3(0.625, 0.25, 0.125)) - 1.0, 1.0, 0.0, 1.0);
}

float cloudDensity(vec3 coords, float ph, float wh, float wd, float gc, float gd) {
    return clamp(remap(sampleShapeNoise(coords) * cloudShapeAlter(ph, wh), 1 - gc * weatherMapCoverage(coords.xz, gc), 1.0, 0.0, 1.0), 0.0, 1.0) * cloudDensityAlter(ph, gd, wd);
}

const float FOV = radians(45.0);
const float cloudMinHeight = 400.0;
const float cloudMaxHeight = 1000.0;
const float stepIncRate = 0.1;

void main() {
    vec4 noiseTestSample = texture(weatherMap, fract(TexCoords * 2.0)); //4 weather map channels
    //noiseTestSample = texture(shapeNoise, vec3(fract(TexCoords.x * 2.0), TexCoords.y, fract(TexCoords.y * 2.0))); //4 shape noise channels
    //FragColor = vec4(texture(shapeNoise, vec3(TexCoords, 1.0)).rrr, 1.0);

    //if (TexCoords.x < 0.5 && TexCoords.y >= 0.5) FragColor = vec4(noiseTestSample.rrr, 1.0);
    //else if (TexCoords.x < 0.5 && TexCoords.y < 0.5) FragColor = vec4(noiseTestSample.bbb, 1.0);
    //else if (TexCoords.x >= 0.5 && TexCoords.y >= 0.5) FragColor = vec4(noiseTestSample.ggg, 1.0);
    //else FragColor = vec4(noiseTestSample.aaa, 1.0);

    //FragColor = vec4(texture(weatherMap, TexCoords).rgb, 1.0);
    //FragColor = vec4(vec3(weatherMapCoverage(TexCoords, 1.0)), 1.0);


    vec2 rayAngle = (TexCoords - vec2(0.5)) * (vec2(1.0, invAspectRatio) * FOV); //Angle away from the center of the camera
    rayAngle += radians(camAngle);
    //if (rayAngle.y < 0.0) discard;
    vec3 rayUnitVec = normalize(vec3(sin(rayAngle.x) * cos(rayAngle.y), sin(rayAngle.y), cos(rayAngle.x) * cos(rayAngle.y)));

    vec3 rayPos = vec3( //Extend ray to the edge of the cloud layer
        camPos.x + sin(rayAngle.x) * cos(rayAngle.y),
        cloudMinHeight,
        camPos.z + cos(rayAngle.x) * cos(rayAngle.y)
    );
    rayPos = camPos + rayUnitVec * ((cloudMinHeight - camPos.y) / rayUnitVec.y);
    //FragColor = vec4(vec3(weatherMapCoverage(rayPos.xz, 0.8)), 1.0);
    //FragColor = vec4(abs(rayPos.xz) / 10000.0, 1.0, 1.0);

    float stepOrig = distance(camPos, rayPos);
    //vec3 stepNew = stepOrig + distance(camPos, rayPos) * stepIncRate;
    //FragColor = vec4(vec3(1.0) - vec3(0.3255, 0.4863, 0.5608), 1.0);
    //if (camAngle.y > 0.0) FragColor = vec4(vec3(0.3255, 0.4863, 0.5608), 1.0);
    const int samples = 50;
    float stepScale = 1.0; //0.5 when in a cloud
    int outOfCloudSteps = 0; //Advances when emerging from within a cloud
    float totalDensity = 0.0;
    for (int i = 0; i<samples; i++) {
        vec3 stepVec = rayUnitVec * 5.0 * (100.0 / float(samples)) * (stepOrig / 400.0) * stepScale;
        rayPos += stepVec;
        vec4 weatherMapSampleVal = texture(weatherMap, rayPos.xz);
        float wh = weatherMapSampleVal.b;
        float wd = weatherMapSampleVal.a;
        float ph = clamp((rayPos.y - cloudMinHeight) / (cloudMaxHeight - cloudMinHeight), 0.0, 1.0); //percentage of ray height between cloud height bounds
        float sampleDensity = cloudDensity(rayPos, ph, wh, wd, 0.7, 1.0);
        if (sampleDensity > 0.0) {
            outOfCloudSteps = 0;
            rayPos -= stepVec;
            weatherMapSampleVal = texture(weatherMap, rayPos.xz);
            wh = weatherMapSampleVal.b;
            wd = weatherMapSampleVal.a;
            ph = clamp((rayPos.y - cloudMinHeight) / (cloudMaxHeight - cloudMinHeight), 0.0, 1.0); //percentage of ray height between cloud height bounds
            sampleDensity = cloudDensity(rayPos, ph, wh, wd, 0.7, 1.0);
            stepScale = 0.9;
        }
        else if (stepScale < 1.0) {
            if (outOfCloudSteps >= 4) stepScale = 1.0;
            else outOfCloudSteps++;
        }
        totalDensity += sampleDensity;
        /*if ((sampleDensity) > 0.01) {
            FragColor = vec4(1.0);
            break;
        }*/
    }
    FragColor = vec4(vec3(totalDensity / float(samples)), 1.0); //MAIN DRAWING LINE

    vec4 weatherMapSampleVal = texture(weatherMap, TexCoords);
    float wh = weatherMapSampleVal.b;
    float wd = weatherMapSampleVal.a;
    float ph = clamp((testSampleHeight - cloudMinHeight) / (cloudMaxHeight - cloudMinHeight), 0.0, 1.0);
    //FragColor = vec4(vec3(cloudDensity(vec3(TexCoords.x, testSampleHeight, TexCoords.y), ph, wh, wd, 0.9, 1.0)), 1.0);
    //FragColor = vec4(vec3(sampleShapeNoise(vec3(TexCoords.x, testSampleHeight, TexCoords.y))), 1.0);

    //FragColor = vec4(vec3(weatherMapCoverage(TexCoords * 10000.0, 1.0)), 1.0);
}