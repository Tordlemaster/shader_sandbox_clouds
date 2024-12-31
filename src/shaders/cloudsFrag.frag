#version 460 core

in vec2 TexCoords;
in vec2 FragPos;

out vec4 FragColor;

uniform float invAspectRatio;
uniform sampler2D weatherMap;
uniform sampler3D shapeNoise;
uniform sampler3D detailNoise;
uniform sampler2D blueNoise;

uniform float testSampleHeight;
uniform float exposure;
uniform float detailScale;
uniform float hg;

layout (std140, binding = 0) uniform camera {
    vec2 camAngle;
    vec3 camPos;
};

const float PI = 3.14159265358979;

float softClip(float x) {
    return (-1.0 / (2.0 * x + 1.0)) + 1.0;
}

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

float sampleShapeNoise(vec3 coords, float mipmapLevel) {
    vec4 SNsample = texture(shapeNoise, coords / 1000.0, mipmapLevel);
    return remap(SNsample.r, dot(SNsample.gba, vec3(0.625, 0.25, 0.125)) - 1.0, 1.0, 0.0, 1.0);
}

float sampleDetailNoise(vec3 coords, float mipmapLevel, float gc, float ph) {
    vec4 DNsample = texture(detailNoise, coords / detailScale, mipmapLevel);
    float dn = dot(DNsample.rgb, vec3(0.625, 0.25, 0.125));
    return 0.35 * exp(-gc * 0.75) * mix(dn, 1 - dn, clamp(ph * 5.0, 0.0, 1.0));
}

float cloudDensity(vec3 coords, float mipmapLevel, float gc, float gd, float cloudMinHeight, float cloudMaxHeight) {
    vec4 weatherMapSampleVal = texture(weatherMap, coords.xz / 10000.0, mipmapLevel);
    float coverage = max(weatherMapSampleVal.r, clamp(gc - 0.5, 0.0, 1.0) * weatherMapSampleVal.g * 2);
    float wh = weatherMapSampleVal.b;
    float wd = weatherMapSampleVal.a;
    float ph = clamp((coords.y - cloudMinHeight) / (cloudMaxHeight - cloudMinHeight), 0.0, 1.0); //percentage of ray height between cloud height bounds
    float sn = clamp(remap(sampleShapeNoise(coords, mipmapLevel) * cloudShapeAlter(ph, wh), 1 - gc * coverage, 1.0, 0.0, 1.0), 0.0, 1.0);
    return clamp(remap(sn, sampleDetailNoise(coords, mipmapLevel, gc, ph), 1.0, 0.0, 1.0), 0.0, 1.0) * cloudDensityAlter(ph, gd, wd);
}

float beersLaw(float density, float b) {
    return exp(-b * density) * (1.0 - exp());
}

float clampBeersLaw(float density, float bound, float b) {
    return max(beersLaw(density, b), beersLaw(bound, b));
}

float henyeyGreenstein(vec3 lightDir, vec3 rayDir, float g) {
    return (1/(4*PI)) * ((1 - g*g) / pow(1 + g*g - 2*g*dot(lightDir, rayDir), 1.5));
}

float calcSunStepMipMapValue(int step) {
    return floor(float(step) * 0.5);
}

const float FOV = radians(45.0);
const float cloudMinHeight = 400.0;
const float cloudMaxHeight = 1000.0;
const float stepIncRate = 0.1;
const int sunSteps = 4;
const vec3 sunDir = normalize(vec3(1.0, 1.0, 0.0));
//const float sunStepMipmapLevels[sunSteps] = float[](0.0, 0.0, 1.0, 1.0);

void main() {
    vec4 noiseTestSample = texture(weatherMap, fract(TexCoords * 2.0)); //4 weather map channels
    //noiseTestSample = texture(detailNoise, vec3(fract(TexCoords.x * 2.0), fract(TexCoords.y * 2.0), 2.0)); //4 shape noise channels
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

    //Sum the densities along each full sun-march (each multiplied by the distance covered)
    //Put that through Beer's Law
    //Take the sum of these results across the viewing ray: (each multipled by the distance?)
    //  Each weighted by a running product of Beer's Laws of the regular ray densities so that the visibility of the light attenuates with distance

    const int samples = 500;
    const float sunStepDensityCoeff = 0.1;
    float sunStepLength = ((cloudMaxHeight - cloudMinHeight) * 1.0) / (sunSteps + 1);
    vec3 sunStepVec = sunDir * sunStepLength;
    float totalDensity = 0.0;
    vec4 densityColor = vec4(0.0);
    float lightLevel = 0.0;
    float runningBeerAttenuation = 1.0;
    float rayMarchLength = 3.0 * (100.0 / float(samples)) * (stepOrig / 400.0);
    float shortRayMarchLength = rayMarchLength / 3.0;
    float curRayMarchLength = rayMarchLength;
    rayPos += (texture(blueNoise, TexCoords * 2.0).r - 0.5) * 2.0 * rayMarchLength;
    for (int i = 0; i<samples; i++) {
        rayPos += rayUnitVec * curRayMarchLength;
        float sampleDensity = cloudDensity(rayPos, 0.0, 0.6, 0.8, cloudMinHeight, cloudMaxHeight);
        if (sampleDensity > 0.000001) {
            rayPos -= rayUnitVec * curRayMarchLength;
            curRayMarchLength = shortRayMarchLength;
        }
        else if (sampleDensity < 0.000001) {
            curRayMarchLength = rayMarchLength;
        }

        vec4 color = vec4(mix(vec3(1.0), vec3(0.0), sampleDensity), sampleDensity);
        float diffuseStepLength = 40.0;
        float diffuse = clamp(remap(sampleDensity - 5.0 * cloudDensity(rayPos + diffuseStepLength * sunDir, 0.0, 0.6, 0.8, cloudMinHeight, cloudMaxHeight), -1.0, 1.0, 0.0, 1.0), 0.0, 1.0);
        vec3 diffuseColor = vec3(0.60,0.60,0.75) * 1.1 + 0.8 * vec3(1.0,0.6,0.3) * diffuse;
        color.rgb *= diffuseColor;

        color.rgb *= color.a;
        densityColor += color * (1.0 - densityColor.a);

        if (sampleDensity > 0.000001) { //MARCH TOWARD THE SUN
            vec3 sunStepPos = rayPos + sunStepVec;
            float sunSampleTotalDensity = 0.0;
            for (int j = 0; j < sunSteps - 1; j++) {
                float sunSampleDensity = cloudDensity(sunStepPos, calcSunStepMipMapValue(j), 0.6, 0.8, cloudMinHeight, cloudMaxHeight);
                sunSampleTotalDensity += sunSampleDensity * sunStepLength * sunStepDensityCoeff;
                //lightLevel += (1 - sunSampleDensity);
                sunStepPos += sunStepVec;
            }
            //lightLevel += beersLaw(sunSampleTotalDensity, testSampleHeight);// * clamp(1.0 - totalDensity, 0.0, 1.0);
            float sunMarchLightAttenuation = beersLaw(sunSampleTotalDensity, testSampleHeight);
            lightLevel += runningBeerAttenuation * sunMarchLightAttenuation * curRayMarchLength;
            runningBeerAttenuation *= beersLaw(sampleDensity * curRayMarchLength * sunStepDensityCoeff, testSampleHeight);
        }
        totalDensity += (sampleDensity * 20.0) * curRayMarchLength; //TODO MULTIPLY BY LENGTH???
        if (totalDensity > 1.0){
            totalDensity = 1.0;
            break;
        }
    }
    totalDensity = min(totalDensity, 1.0);
    //FragColor = vec4(vec3(softClip(totalDensity)), 1.0); //MAIN DRAWING LINE
    //FragColor = vec4(densityColor.rgb, 1.0);
    vec3 finalColor = vec3(0.8431, 0.9608, 1.0) * (lightLevel * henyeyGreenstein(-sunDir, rayUnitVec, hg));
    finalColor = vec3(0.5412, 0.7686, 0.9961) * 2.0 * (1-totalDensity) + finalColor;
    //finalColor /= (finalColor + 1); //REINHARD TONEMAPPING
    finalColor = vec3(1.0) - exp(-finalColor * exposure); //EXPOSURE TONEMAPPING
    //finalColor = pow(finalColor, vec3(1.0/2.2)); //GAMMA CORRECTION
    FragColor = vec4(finalColor, 1.0);
    //FragColor = vec4(vec3(totalDensity), 1.0);


    //if (totalDensity < 0.01) FragColor = vec4(0.3412, 0.5647, 0.8392, 1.0);
    //FragColor = vec4(0.3216, 0.5255, 0.7765, 1.0) * (1.0 - totalDensity) + vec4(vec3(lightLevel / 20.0), 1.0);

    //FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    //if (totalDensity > 0.0) FragColor = vec4(1.0, 1.0, 1.0, 1.0);

    vec4 weatherMapSampleVal = texture(weatherMap, TexCoords);
    float wh = weatherMapSampleVal.b;
    float wd = weatherMapSampleVal.a;
    float ph = clamp((testSampleHeight - cloudMinHeight) / (cloudMaxHeight - cloudMinHeight), 0.0, 1.0);
    //FragColor = vec4(vec3(cloudDensity(vec3(TexCoords.x, testSampleHeight, TexCoords.y), ph, wh, wd, 0.9, 1.0)), 1.0);
    //FragColor = vec4(vec3(sampleShapeNoise(vec3(TexCoords.x, testSampleHeight, TexCoords.y))), 1.0);

    //FragColor = vec4(vec3(weatherMapCoverage(TexCoords * 10000.0, 1.0)), 1.0);

    //FragColor = vec4(vec3(0.0), 1.0);
    //if (abs(TexCoords.y - beersLaw(TexCoords.x, testSampleHeight)) < 0.001) FragColor.g = 1.0;
    //if (abs(TexCoords.y - clampBeersLaw(TexCoords.x, 0.9, testSampleHeight)) < 0.001) FragColor.b = 1.0;
}