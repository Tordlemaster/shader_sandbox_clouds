#version 460 core

in vec2 TexCoords;
in vec2 FragPos;

out vec4 FragColor;

uniform float invAspectRatio;
uniform sampler2D weatherMap;
uniform sampler3D shapeNoise;
uniform sampler3D detailNoise;
uniform sampler2D blueNoise;

uniform float b;
uniform float exposure;
uniform float detailScale;
uniform float hg;

uniform int cloudFrame; //0 - 15
uniform vec2 resolution; //WIDTH, HEIGHT

layout (std140, binding = 0) uniform camera {
    vec2 camAngle;
    vec2 camMotion;
    vec3 camPos;
};

const float PI = 3.14159265358979;
const vec2[16] cloudFramePxOffsets = vec2[16](
    vec2(0.0, 3.0),
    vec2(2.0, 1.0),
    vec2(2.0, 3.0),
    vec2(0.0, 1.0),
    vec2(1.0, 2.0),
    vec2(3.0, 0.0),
    vec2(3.0, 2.0),
    vec2(1.0, 0.0),
    vec2(1.0, 3.0),
    vec2(3.0, 1.0),
    vec2(3.0, 3.0),
    vec2(1.0, 1.0),
    vec2(0.0, 2.0),
    vec2(2.0, 0.0),
    vec2(2.0, 2.0),
    vec2(0.0, 0.0)
);
const vec3 SKY_COLOR = vec3(0.369, 0.663, 1.0);

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
    vec4 SNsample = texture(shapeNoise, coords / 700.0, mipmapLevel);
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

float beersLaw(float distance) {
    return exp(-b * distance);
}

float inScatter(float distance) {
    return 1.0 - exp(-1.0 * distance);
}

vec3 reinhardTonemapping(vec3 color) {
    float luminosity = (color.r + color.g + color.b) / 3.0;
    return color / (1.0 + luminosity);
}

const float FOV = radians(45.0);
const float cloudMinHeight = 400.0;
const float cloudMaxHeight = 1000.0;
const float maxRayDistance = 6000.0;
const int sunSteps = 8;
const float sunStepLength = ((cloudMaxHeight - cloudMinHeight) * 0.5) / float(sunSteps);
const vec3 sunDir = normalize(vec3(1.0, 1.0, 0.0));

float rayLenOutside = 50.0;
float rayLenInside = 10.0;

int inStepsInAnOutStep = int(rayLenOutside / rayLenInside + 1);
int inStepsCloudCount = inStepsInAnOutStep; //PREVENTS RAYMARCHER FROM SWITCHING BACK TO LONG STEPS AFTER ONLY ONE SMALL STEP, CORRUPTING THE TOTAL DENSITY

void main() {
    //SET UP RAYS
    vec2 pxCoords = TexCoords + (cloudFramePxOffsets[min(cloudFrame, 15)] / (resolution));
    vec2 rayAngle = (pxCoords - vec2(0.5)) * (vec2(1.0, invAspectRatio) * FOV); //Angle away from the center of the camera
    rayAngle += radians(camAngle);
    vec3 rayUnitVec = normalize(vec3(sin(rayAngle.x) * cos(rayAngle.y), sin(rayAngle.y), cos(rayAngle.x) * cos(rayAngle.y)));

    float initialRayDist = 0.0;
    if (camPos.y < cloudMinHeight) initialRayDist = ((cloudMinHeight - camPos.y) / rayUnitVec.y);
    else if (camPos.y > cloudMaxHeight) initialRayDist = ((camPos.y - cloudMaxHeight) / rayUnitVec.y);

    vec3 rayPos = camPos + initialRayDist * rayUnitVec;
    rayPos += rayUnitVec * 15.0 * texture(blueNoise, pxCoords * (resolution / vec2(470))).r;

    //rayLenOutside = ((3.0 * (1.0 - abs(rayAngle.y))) + 1.0) * rayLenOutside;
    //rayLenInside = rayLenOutside / 4.0;

    //MAIN RAYMARCHING
    float totalDensity = 0.0;
    float rayDist = 0.0;
    float distWithinCloud = 0.0;
    float totalAttenuation = 0.0;
    float mainRaySample = 0.0;
    float activeRayLen = rayLenOutside;
    
    while (rayDist <= maxRayDistance && totalDensity < 1.0) {
        mainRaySample = cloudDensity(rayPos, 0.0, 0.8, 0.3, cloudMinHeight, cloudMaxHeight);

        if (mainRaySample > 0.0000001 && activeRayLen == rayLenOutside) { //SWITCH TO IN-CLOUD MODE
            if ((rayDist + initialRayDist)> rayLenOutside) {
                rayPos -= rayUnitVec * (rayLenOutside - rayLenInside); //STEP BACK BY RAYLENOUTSIDE AND FORWARD BY RAYLENINSIDE
                rayDist -= (rayLenOutside - rayLenInside);
                mainRaySample = cloudDensity(rayPos, 0.0, 0.8, 0.3, cloudMinHeight, cloudMaxHeight);
            }
            activeRayLen = rayLenInside;
        }

        totalDensity = min(totalDensity + mainRaySample * activeRayLen, 1.0); //ADD DENSITY SAMPLE TO TOTAL
        //OUT OF CLOUD MODE SWITCH NEEDS TO BE BELOW TOTALDENSITY BECAUSE OTHERWISE CLOUD EXITS WILL GET THE DENSITY OF A LARGE STEP
        if (mainRaySample < 0.0000001) { //SWITCH TO OUT-OF-CLOUD MODE
            activeRayLen = rayLenOutside;
        }
        vec3 sunRayPos = rayPos;
        float totalSunDensity = 0.0;
        float sunSampleDensity;
        if (activeRayLen == rayLenInside) { //STEPS TOWARD THE SUN
            for (int i=0; i<sunSteps; i++) { //STEPS TOWARD THE SUN
                sunRayPos += sunDir * sunStepLength;
                sunSampleDensity = cloudDensity(sunRayPos, 0.0, 0.8, 0.3, cloudMinHeight, cloudMaxHeight); //SUN RAY DENSITY SAMPLE
                totalSunDensity += sunSampleDensity * sunStepLength; //MULTIPLY BY DISTANCE COVERED BECAUSE BEER'S LAW USES DISTANCE
            }

            float totalSunAttenuation = beersLaw(totalSunDensity); //HOW MUCH LIGHT REACHES THE MAIN RAY SAMPLE POINT
            totalAttenuation += (totalSunAttenuation * beersLaw(totalDensity)); //HOW MUCH LIGHT FROM THIS MAIN RAY SAMPLE POINT REACHES THE CAMERA
        }
        rayPos += rayUnitVec * activeRayLen;
        rayDist += activeRayLen;
    }

    //vec3 cloudColor = vec3(totalAttenuation);
    vec3 cloudColor = vec3(0.263, 0.333, 0.459) + (vec3(1.0, 0.871, 0.616) * 3.0) * totalAttenuation; //cloud shadow color + cloud light color
    vec3 totalColor = cloudColor * totalDensity + SKY_COLOR * 2.0 * (1 - totalDensity);

    totalColor = reinhardTonemapping(totalColor); //REINHARD TONEMAPPING
    totalColor = pow(totalColor, vec3(1.0/2.2)); //GAMMA CORRECTION

    FragColor = vec4(totalColor, 1.0);
}

void main2() {
    vec4 noiseTestSample = texture(weatherMap, fract(TexCoords * 2.0)); //4 weather map channels
    noiseTestSample = texture(detailNoise, vec3(fract(TexCoords.x * 2.0), fract(TexCoords.y * 2.0), 2.0)); //4 shape noise channels
    //FragColor = vec4(texture(shapeNoise, vec3(TexCoords, 1.0)).rrr, 1.0);

    if (TexCoords.x < 0.5 && TexCoords.y >= 0.5) FragColor = vec4(noiseTestSample.rrr, 1.0);
    else if (TexCoords.x < 0.5 && TexCoords.y < 0.5) FragColor = vec4(noiseTestSample.bbb, 1.0);
    else if (TexCoords.x >= 0.5 && TexCoords.y >= 0.5) FragColor = vec4(noiseTestSample.ggg, 1.0);
    else FragColor = vec4(noiseTestSample.aaa, 1.0);

    //FragColor = vec4(texture(weatherMap, TexCoords).rgb, 1.0);
    //FragColor = vec4(vec3(weatherMapCoverage(TexCoords, 1.0)), 1.0);
}