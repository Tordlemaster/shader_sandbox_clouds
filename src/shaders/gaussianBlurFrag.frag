#version 460 core

uniform sampler2D fbo;
uniform bool horizontal;

in vec2 TexCoords;
out vec4 FragColor;

const float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
const float blurSize = 0.003;

void main() {
    float result = texture(fbo, TexCoords).r * weight[0];
    
    vec2 axis = vec2(0.0, blurSize);
    if (horizontal) axis = vec2(blurSize, 0.0);

    for (int i=1; i<5; i++) {
        result += texture(fbo, TexCoords + axis * i).r * weight[i];
        result += texture(fbo, TexCoords - axis * i).r * weight[i];
    }

    FragColor = vec4(result, texture(fbo, TexCoords).gba);
}