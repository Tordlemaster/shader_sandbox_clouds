#version 460 core

in vec2 TexCoords;
in vec2 FragPos;

out vec4 FragColor;

layout (std140, binding = 0) uniform camera {
    vec2 camAngle;
    vec2 camMotion;
    vec3 camPos;
};

uniform int cloudFrame;
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
uniform vec2 resolution;

uniform sampler2D cloudData;
uniform sampler2D previousFrame;

const float FOV = 45.0;

void main() {
    vec2 pxCoord = mod((TexCoords * resolution), 4.0);
    if (floor(pxCoord) == floor(cloudFramePxOffsets[cloudFrame])) {
        FragColor = texture(cloudData, TexCoords);
    }
    else {
        FragColor = texture(previousFrame, TexCoords + (camMotion / FOV));
    }
    //FragColor = vec4(pxCoord/4.0, 0.0, 1.0);
    //FragColor = texture(cloudData, TexCoords);
}