#version 460 core

in vec2 TexCoords;
in vec2 FragPos;

out vec4 FragColor;

uniform sampler2D FBO;

void main() {
    //FragColor = vec4(TexCoords, 0.0, 1.0);
    //FragColor.r = float(abs(FragPos.y - sin(FragPos.x * 4.0)) < 0.01);
    FragColor = texture(FBO, TexCoords);
    //FragColor = vec4(vec3(texture(FBO, TexCoords).g), 1.0);
}