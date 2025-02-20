#include <iostream>
#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "FastNoiseLite.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "shader_reader.h"

float FBTriangleVertices[] = {
    //pos       //texcoords
    -1.0, -1.0, 0.0, 0.0,
    3.0, -1.0, 2.0, 0.0,
    -1.0, 3.0, 0.0, 2.0
};

const int SCR_WIDTH = 1000;
const int SCR_HEIGHT = 1000;

typedef struct {
    unsigned char r, g, b, a;
} uchar_vec4;

struct camera{
    glm::vec2 angle = {0.0, 30.0};
    glm::vec2 motion = {0.0, 0.0};
    glm::vec3 pos = {0.0, 0.0, 0.0};
    glm::vec3 forward;
    glm::vec3 up = {0.0, 1.0, 0.0};
} cam;

unsigned char floatToByte(float val) {
    return (unsigned char)(val * 255.0);
}

int setupOpenGL(GLFWwindow **window) {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Shader Sandbox", NULL, NULL);
    if (*window == NULL) {
        std::cout << "Failed to create window" << std::endl;
        glfwTerminate();
        exit(-1);
    }
    glfwMakeContextCurrent(*window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        exit(-1);
    }

    return 0;
}

void updateCameraBuffer(unsigned int camUBO, camera &cam) {
    glBindBuffer(GL_UNIFORM_BUFFER, camUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 2 * sizeof(float), &cam.angle);
    glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(float), 2 * sizeof(float), &cam.motion);
    glBufferSubData(GL_UNIFORM_BUFFER, 4 * sizeof(float), 3 * sizeof(float), glm::value_ptr(cam.pos));
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    static float lastX = 400;
    static float lastY = 300;

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    const float sensitivity = 0.1f;
    yoffset *= sensitivity;
    xoffset *= sensitivity;

    cam.angle.x += xoffset;
    cam.angle.y += yoffset;

    cam.motion.x = xoffset;
    cam.motion.y = yoffset;

    if (cam.angle.y > 89.0f) cam.angle.y = 89.0f;
    else if (cam.angle.y < -89.0f) cam.angle.y = -89.0f;
    
}

float weatherMapSigmoid(float x) {
    return 1.0 / (1 + exp(-8.0 * (x - 0.5)));
}

int main() {
    GLFWwindow *window;
    //SET UP OPENGL
    setupOpenGL(&window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    //CAMERA
    unsigned int camUBO;
    glGenBuffers(1, &camUBO);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, camUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(float) * 8, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    //SET UP MOUSE INPUT
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);

    //CREATE VAOs
    unsigned int FBTriVAO, FBTriVBO;
    glGenVertexArrays(1, &FBTriVAO);
    glGenBuffers(1, &FBTriVBO);
    glBindVertexArray(FBTriVAO);
    glBindBuffer(GL_ARRAY_BUFFER, FBTriVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(FBTriangleVertices), FBTriangleVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));


    //CREATE FRAMEBUFFERS
    unsigned int mainFBO;
    glGenFramebuffers(1, &mainFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, mainFBO);

    unsigned int mainColorbuffer;
    glGenTextures(1, &mainColorbuffer);
    glBindTexture(GL_TEXTURE_2D, mainColorbuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mainColorbuffer, 0);
    
    unsigned int mainRBO;
    glGenRenderbuffers(1, &mainRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, mainRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mainRBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer incomplete." << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    unsigned int cloudFBO, cloudReprojFBO0, cloudReprojFBO1;
    glGenFramebuffers(1, &cloudFBO);
    glGenFramebuffers(1, &cloudReprojFBO0);
    glGenFramebuffers(1, &cloudReprojFBO1);

    unsigned int cloudFBOTex, cloudReprojFBOTex0, cloudReprojFBOTex1;
    glGenTextures(1, &cloudFBOTex);
    glBindTexture(GL_TEXTURE_2D, cloudFBOTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCR_WIDTH/4, SCR_HEIGHT/4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, cloudFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cloudFBOTex, 0);

    glGenTextures(1, &cloudReprojFBOTex0);
    glBindTexture(GL_TEXTURE_2D, cloudReprojFBOTex0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, cloudReprojFBO0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cloudReprojFBOTex0, 0);

    glGenTextures(1, &cloudReprojFBOTex1);
    glBindTexture(GL_TEXTURE_2D, cloudReprojFBOTex1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, cloudReprojFBO1);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cloudReprojFBOTex1, 0);

    //SET UP SHADERS
    Shader fboShader = Shader(".\\shaders\\fboVert.vert", ".\\shaders\\fboFrag.frag");
    Shader blurShader = Shader(".\\shaders\\fboVert.vert", ".\\shaders\\gaussianBlurFrag.frag");
    Shader cloudShader = Shader(".\\shaders\\fboVert.vert", ".\\shaders\\cloudsFrag3.frag");
    Shader cloudReprojShader = Shader(".\\shaders\\fboVert.vert", ".\\shaders\\cloudsFragReproj.frag");

    //NOISE TEXTURES
    /*FastNoiseLite perlin, worley, worleyMod;
    perlin.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    worley.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    worleyMod.SetNoiseType(FastNoiseLite::NoiseType_Cellular);

    perlin.SetFrequency(0.007);
    perlin.SetFractalType(FastNoiseLite::FractalType_FBm);
    perlin.SetFractalOctaves(5);
    perlin.SetFractalLacunarity(2.180);
    perlin.SetFractalGain(0.5);
    perlin.SetFractalWeightedStrength(-0.550);
    worleyMod.SetFrequency(0.048);
    worley.SetFrequency(0.05);
    worley.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Euclidean);
    worley.SetCellularReturnType(FastNoiseLite::CellularReturnType_Distance);
    glm::vec4 *weatherMapData = (glm::vec4*) malloc(512 * 512 * sizeof(glm::vec4));
    int index = 0;
    for (int y=0; y<512; y++) {
        for (int x=0; x<512; x++) {
            weatherMapData[index] = {
                5.0 * (1.0 - (worley.GetNoise((float)x, (float)y) * 0.5 + 0.5)) - 4.0,
                perlin.GetNoise((float)x, (float)y) * 0.5 + 0.5 - (worleyMod.GetNoise((float)x, (float)y) * 0.5 + 0.5) * 0.4,
                1.0,
                0.5
            };
            //weatherMapData[index] = {1.0, 1.0, 1.0, 1.0};
            index++;
        }
    }
    
    unsigned int weatherMapTex;
    glGenTextures(1, &weatherMapTex);
    glBindTexture(GL_TEXTURE_2D, weatherMapTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_FLOAT, weatherMapData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    free(weatherMapData);*/

    FastNoiseLite perlin_r, worley_r, worley_g, worley_b, worley_a;
    perlin_r.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    perlin_r.SetFrequency(0.060);
    perlin_r.SetFractalType(FastNoiseLite::FractalType_FBm);
    perlin_r.SetFractalOctaves(5);
    perlin_r.SetFractalGain(0.29);
    perlin_r.SetFractalWeightedStrength(-1.73);
    worley_r.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    worley_r.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_EuclideanSq);
    worley_r.SetFrequency(0.04);
    worley_g.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    worley_g.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Euclidean);
    worley_g.SetFractalType(FastNoiseLite::FractalType_FBm);
    worley_g.SetFractalGain(0.22);
    worley_g.SetFractalWeightedStrength(-1.2);
    worley_g.SetFrequency(0.07);
    worley_g.SetFractalOctaves(3);
    worley_b.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    worley_b.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Euclidean);
    worley_b.SetFractalType(FastNoiseLite::FractalType_FBm);
    worley_b.SetFractalGain(0.26);
    worley_b.SetFractalWeightedStrength(-1.2);
    worley_b.SetFrequency(0.10);
    worley_b.SetFractalOctaves(3);
    worley_a.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    worley_a.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Euclidean);
    worley_a.SetFractalType(FastNoiseLite::FractalType_FBm);
    worley_a.SetFractalGain(0.28);
    worley_a.SetFractalWeightedStrength(-1.2);
    worley_a.SetFrequency(0.14);
    worley_a.SetFractalOctaves(3);
    glm::vec4 *perlinNoiseData = (glm::vec4*)malloc(128 * 128 * 128 * sizeof(glm::vec4));
    int index = 0;
    for (int z=0; z<128; z++) {
        for (int y=0; y<128; y++) {
            for (int x=0; x<128; x++) {
                perlinNoiseData[index] = {
                    (perlin_r.GetNoise((float)x, (float)y, (float)z) * 0.5f + 0.5f) * (1.0f - (worley_r.GetNoise((float)x, (float)y, (float)z) + 1.0f)),
                    1.0f - (worley_g.GetNoise((float)x, (float)y, (float)z) + 1.0f),
                    1.0f - (worley_b.GetNoise((float)x, (float)y, (float)z) + 1.0f),
                    1.0f - (worley_a.GetNoise((float)x, (float)y, (float)z) + 1.0f)
                };
                index++;
            }
        }
    }

    unsigned int shapeNoiseTex;
    glGenTextures(1, &shapeNoiseTex);
    glBindTexture(GL_TEXTURE_3D, shapeNoiseTex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 128, 128, 128, 0, GL_RGBA, GL_FLOAT, perlinNoiseData);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    free(perlinNoiseData);

    FastNoiseLite detailWorleyR, detailWorleyG, detailWorleyB;
    int detailNoiseSize = 32;
    detailWorleyR.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    detailWorleyR.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_EuclideanSq);
    detailWorleyR.SetFrequency(0.0085 * 16);
    detailWorleyR.SetFractalType(FastNoiseLite::FractalType_FBm);
    detailWorleyR.SetFractalOctaves(3);
    detailWorleyR.SetFractalGain(0.33);
    detailWorleyR.SetFractalWeightedStrength(0.0);
    detailWorleyG.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    detailWorleyG.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Euclidean);
    detailWorleyG.SetFrequency(0.012 * 16);
    detailWorleyG.SetFractalType(FastNoiseLite::FractalType_FBm);
    detailWorleyG.SetFractalOctaves(5);
    detailWorleyG.SetFractalGain(0.45);
    detailWorleyG.SetFractalWeightedStrength(-0.55);
    detailWorleyB.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    detailWorleyB.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Euclidean);
    detailWorleyB.SetFrequency(0.032 * 16);
    detailWorleyB.SetFractalType(FastNoiseLite::FractalType_FBm);
    detailWorleyB.SetFractalOctaves(3);
    detailWorleyB.SetFractalGain(0.39);
    detailWorleyB.SetFractalWeightedStrength(-0.7);
    glm::vec4 *detailNoiseData = (glm::vec4*) malloc(detailNoiseSize * detailNoiseSize * detailNoiseSize * sizeof(glm::vec4));
    index = 0;
    for (int z=0; z<detailNoiseSize; z++) {
        for (int x=0; x<detailNoiseSize; x++) {
            for (int y=0; y<detailNoiseSize; y++) {
                detailNoiseData[index] = {
                    1.0 - (detailWorleyR.GetNoise((float)x, (float)y, (float)z) + 1.0),
                    1.0 - (detailWorleyG.GetNoise((float)x, (float)y, (float)z) + 1.0),
                    1.0 - (detailWorleyB.GetNoise((float)x, (float)y, (float)z) + 1.0),
                    0.0
                };
                index++;
            }
        }
    }

    unsigned int detailNoiseTex;
    glGenTextures(1, &detailNoiseTex);
    glBindTexture(GL_TEXTURE_3D, detailNoiseTex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, detailNoiseSize, detailNoiseSize, detailNoiseSize, 0, GL_RGBA, GL_FLOAT, detailNoiseData);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    free(detailNoiseData);

    //DISPATCH COMPUTE SHADERS TO GENERATE NOISE
    Shader weatherMapComputeShader = Shader(".\\shaders\\cloudNoise2DGen.comp");
    Shader shapeNoiseComputeShader = Shader(".\\shaders\\cloudNoise3DGen.comp");

    int tex_w = 512, tex_h = 512;
    GLuint weatherMapShaderTex0, weatherMapShaderTex1;
    glGenTextures(1, &weatherMapShaderTex1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, weatherMapShaderTex1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, tex_w, tex_h, 0, GL_RGBA, GL_FLOAT, NULL);

    glGenTextures(1, &weatherMapShaderTex0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, weatherMapShaderTex0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, tex_w, tex_h, 0, GL_RGBA, GL_FLOAT, NULL);

    glBindImageTexture(0, weatherMapShaderTex0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    weatherMapComputeShader.use();
    glDispatchCompute(tex_w, tex_h, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    GLuint weatherMapFBOs[2];
    glGenFramebuffers(2, weatherMapFBOs);
    glBindFramebuffer(GL_FRAMEBUFFER, weatherMapFBOs[0]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, weatherMapShaderTex0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, weatherMapFBOs[1]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, weatherMapShaderTex1, 0);

    blurShader.use();
    glViewport(0, 0, tex_w, tex_h);
    glBindFramebuffer(GL_FRAMEBUFFER, weatherMapFBOs[1]);
    blurShader.setBool("horizontal", 0);
    blurShader.setInt("fbo", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, weatherMapShaderTex0);
    glBindVertexArray(FBTriVAO);
    //glDrawArrays(GL_TRIANGLES, 0, 3);
    glViewport(0, 0, tex_w, tex_h);
    glBindFramebuffer(GL_FRAMEBUFFER, weatherMapFBOs[0]);
    blurShader.setBool("horizontal", 1);
    blurShader.setInt("fbo", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, weatherMapShaderTex1);
    glBindVertexArray(FBTriVAO);
    //glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindTexture(GL_TEXTURE_2D, weatherMapShaderTex0);
    glGenerateMipmap(GL_TEXTURE_2D);

    int tex_d = 128;
    tex_w = 128;
    tex_h = 128;
    GLuint shapeNoiseShaderTex;
    glGenTextures(1, &shapeNoiseShaderTex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, shapeNoiseShaderTex);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, tex_w, tex_h, tex_d, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindImageTexture(0, shapeNoiseShaderTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    //shapeNoiseComputeShader.use();
    //glDispatchCompute(tex_w, tex_h, tex_d);
    //glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glBindTexture(GL_TEXTURE_3D, shapeNoiseTex); //TODO TODO REPLACE WITH SHADER VERSION WHEN FIXING SHADER
    glGenerateMipmap(GL_TEXTURE_3D);


    glBindTexture(GL_TEXTURE_3D, detailNoiseTex);
    glGenerateMipmap(GL_TEXTURE_3D);

    //LOAD ASSETS
    int txHeight, txWidth, nrChannels;
    unsigned char* txData = stbi_load("..\\assets\\BlueNoise470.png", &txWidth, &txHeight, &nrChannels, 0);
    //std::cout << nrChannels << std::endl;
    //exit(0);
    unsigned int blueNoiseTexture;
    glGenTextures(1, &blueNoiseTexture);
    glBindTexture(GL_TEXTURE_2D, blueNoiseTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, txWidth, txHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, txData);
    stbi_image_free(txData);

    //PREP
    cloudShader.use();
    cloudShader.setInt("weatherMap", 0);
    cloudShader.setInt("shapeNoise", 1);
    cloudShader.setInt("detailNoise", 2);
    cloudShader.setInt("blueNoise", 3);
    cloudShader.setVec2("resolution", glm::vec2((float)SCR_WIDTH, (float)SCR_HEIGHT));
    cloudShader.setFloat("invAspectRatio", ((float)SCR_HEIGHT / (float)SCR_WIDTH));
    cloudReprojShader.use();
    cloudReprojShader.setVec2("resolution", glm::vec2((float)SCR_WIDTH, (float)SCR_HEIGHT));
    float testSampleHeight = 3.61;
    float exposure = 0.5;
    float detailScale = 134.74; //29.2; //1502.29;
    float hg = 0.8;

    //MAIN LOOP
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    float prevTime = 0.0;
    float currentTime;
    float deltaTime;
    int cloudFrame = 0; //0 - 15, for motion re-projection
    short whichCloudReprojFBO = 0;
    while (!glfwWindowShouldClose(window)) {
        currentTime = glfwGetTime();
        deltaTime = currentTime - prevTime;
        prevTime = currentTime;

        cam.forward = glm::normalize(glm::vec3(
            sin(glm::radians(cam.angle.x)) * cos(glm::radians(cam.angle.y)),
            sin(glm::radians(cam.angle.y)),
            cos(glm::radians(cam.angle.x)) * cos(glm::radians(cam.angle.y))
        ));

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) testSampleHeight += 1.0 * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) testSampleHeight -= 1.0 * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) exposure += 1.0 * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) exposure -= 1.0 * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) detailScale += 50.0 * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) detailScale -= 50.0 * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) hg += 0.03 * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) hg -= 0.03 * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cam.pos += cam.forward * deltaTime * 500.0f;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cam.pos -= cam.forward * deltaTime * 500.0f;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cam.pos += glm::cross(cam.forward, glm::vec3(0.0, 1.0, 0.0)) * deltaTime * 500.0f;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cam.pos -= glm::cross(cam.forward, glm::vec3(0.0, 1.0, 0.0)) * deltaTime * 500.0f;
        //std::cout << cam.pos.x << " " << cam.pos.y << " " << cam.pos.z << "\n";

        //if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        //    cloudFrame = ((cloudFrame + 1) % 16);
        //    std::cout << cloudFrame << "\n";
        //}

        hg = std::max(-1.0f, std::min(1.0f, hg));

        updateCameraBuffer(camUBO, cam);

        glViewport(0, 0, SCR_WIDTH/4, SCR_HEIGHT/4);
        glBindFramebuffer(GL_FRAMEBUFFER, cloudFBO);
        glClear(GL_COLOR_BUFFER_BIT);
        //glClearColor(0.0, 0.0, 0.0, 1.0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, weatherMapShaderTex0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_3D, shapeNoiseTex);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_3D, detailNoiseTex);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, blueNoiseTexture);
        cloudShader.use();
        cloudShader.setInt("weatherMap", 0);
        cloudShader.setInt("shapeNoise", 1);
        cloudShader.setInt("detailNoise", 2);
        cloudShader.setInt("blueNoise", 3);
        cloudShader.setFloat("b", testSampleHeight);
        cloudShader.setFloat("exposure", exposure);
        cloudShader.setFloat("detailScale", detailScale);
        cloudShader.setFloat("hg", hg);
        //cloudShader.setVec2("resolution", glm::vec2(SCR_WIDTH, SCR_HEIGHT));
        cloudShader.setInt("cloudFrame", cloudFrame);
        glBindVertexArray(FBTriVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, whichCloudReprojFBO? cloudReprojFBO0 : cloudReprojFBO1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cloudFBOTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, whichCloudReprojFBO? cloudReprojFBOTex1 : cloudReprojFBOTex0);
        cloudReprojShader.use();
        cloudReprojShader.setInt("cloudFrame", cloudFrame);
        //cloudReprojShader.setVec2("resolution", glm::vec2(SCR_WIDTH, SCR_HEIGHT));
        cloudReprojShader.setInt("cloudData", 0);
        cloudReprojShader.setInt("previousFrame", 1);
        glBindVertexArray(FBTriVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        fboShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, whichCloudReprojFBO? cloudReprojFBOTex0 : cloudReprojFBOTex1);
        //glBindTexture(GL_TEXTURE_2D, weatherMapShaderTex0);
        fboShader.setInt("FBO", 0);
        glBindVertexArray(FBTriVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
        cam.motion = glm::vec2();
        glfwPollEvents();
        
        cloudFrame = ((cloudFrame+1) % 16);
        whichCloudReprojFBO ^= 1;
    }

    std::cout << "Successful" << std::endl;
    std::cout << "b = " << testSampleHeight << std::endl;
    std::cout << "exposure = " << exposure << std::endl;
    std::cout << "detail scale = " << detailScale << std::endl;
    std::cout << "hg = " << hg << std::endl;
    glfwTerminate();
    return 0;
}