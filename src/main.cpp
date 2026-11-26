#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

constexpr GLuint waterGridSubdivisions = 50;
constexpr float waterGridHalfExtent = 5.0F;
constexpr GLsizei causticMapResolution = 256;
constexpr float waterWaveAmplitude = 0.18F;
constexpr float floorHeight = -1.5F;

bool loadShaderSource(const std::string& path, std::string& source)
{
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Failed to open shader file: " << path << '\n';
        return false;
    }

    source.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (file.bad()) {
        std::cerr << "Failed to read shader file: " << path << '\n';
        return false;
    }
    return true;
}

bool expandWaveInclude(std::string& shaderSource, const std::string& waveSource)
{
    const std::string includeDirective = "#include \"wave.glsl\"";
    const std::size_t includePosition = shaderSource.find(includeDirective);
    if (includePosition == std::string::npos) {
        std::cerr << "Failed to find the wave shader include directive.\n";
        return false;
    }

    shaderSource.replace(includePosition, includeDirective.size(), waveSource);
    return true;
}

bool createCausticsTarget(GLuint& framebuffer, GLuint& texture)
{
    glGenFramebuffers(1, &framebuffer);
    glGenTextures(1, &texture);

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_R16F,
        causticMapResolution,
        causticMapResolution,
        0,
        GL_RED,
        GL_FLOAT,
        nullptr
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    const bool isComplete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!isComplete) {
        std::cerr << "Failed to create a complete caustics framebuffer.\n";
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteTextures(1, &texture);
        framebuffer = 0;
        texture = 0;
        return false;
    }

    return true;
}

GLuint compileShader(GLenum type, const std::string& source)
{
    const GLuint shader = glCreateShader(type);
    const char* sourceText = source.c_str();
    glShaderSource(shader, 1, &sourceText, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(logLength), '\0');
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        std::cerr << "Failed to compile shader:\n" << log << '\n';
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint createShaderProgram(const std::string& vertexSource, const std::string& fragmentSource)
{
    const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    if (vertexShader == 0) {
        return 0;
    }

    const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return 0;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == GL_FALSE) {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(logLength), '\0');
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        std::cerr << "Failed to link shader program:\n" << log << '\n';
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

struct GridMesh {
    std::vector<GLfloat> vertices;
    std::vector<GLuint> indices;
};

GridMesh createFlatGrid()
{
    constexpr GLuint verticesPerSide = waterGridSubdivisions + 1;
    constexpr float spacing = (2.0F * waterGridHalfExtent) / waterGridSubdivisions;

    GridMesh mesh;
    mesh.vertices.reserve(static_cast<std::size_t>(verticesPerSide) * verticesPerSide * 3);
    mesh.indices.reserve(
        static_cast<std::size_t>(waterGridSubdivisions) * waterGridSubdivisions * 6
    );

    for (GLuint row = 0; row < verticesPerSide; ++row) {
        for (GLuint column = 0; column < verticesPerSide; ++column) {
            mesh.vertices.push_back(-waterGridHalfExtent + spacing * column);
            mesh.vertices.push_back(0.0F);
            mesh.vertices.push_back(-waterGridHalfExtent + spacing * row);
        }
    }

    for (GLuint row = 0; row < waterGridSubdivisions; ++row) {
        for (GLuint column = 0; column < waterGridSubdivisions; ++column) {
            const GLuint topLeft = row * verticesPerSide + column;
            const GLuint bottomLeft = topLeft + verticesPerSide;
            const GLuint topRight = topLeft + 1;
            const GLuint bottomRight = bottomLeft + 1;

            mesh.indices.insert(mesh.indices.end(), {
                topLeft, bottomLeft, bottomRight,
                topLeft, bottomRight, topRight
            });
        }
    }

    return mesh;
}

void framebufferSizeCallback(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
}

}

int main()
{
    std::string vertexSource;
    std::string fragmentSource;
    std::string causticsVertexSource;
    std::string causticsFragmentSource;
    std::string waveSource;
    const std::string shaderDirectory = WATER_CAUSTICS_SHADER_DIR;
    if (!loadShaderSource(shaderDirectory + "/quad.vert", vertexSource)
        || !loadShaderSource(shaderDirectory + "/quad.frag", fragmentSource)
        || !loadShaderSource(shaderDirectory + "/caustics.vert", causticsVertexSource)
        || !loadShaderSource(shaderDirectory + "/caustics.frag", causticsFragmentSource)
        || !loadShaderSource(shaderDirectory + "/wave.glsl", waveSource)
        || !expandWaveInclude(vertexSource, waveSource)
        || !expandWaveInclude(causticsVertexSource, waveSource)) {
        return EXIT_FAILURE;
    }

    if (glfwInit() != GLFW_TRUE) {
        std::cerr << "Failed to initialize GLFW.\n";
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "WaterCausticsRendering", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create an OpenGL window.\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0) {
        std::cerr << "Failed to load OpenGL functions with GLAD.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    framebufferSizeCallback(window, framebufferWidth, framebufferHeight);

    const GLuint shaderProgram = createShaderProgram(vertexSource, fragmentSource);
    const GLuint causticsProgram = createShaderProgram(causticsVertexSource, causticsFragmentSource);
    if (shaderProgram == 0 || causticsProgram == 0) {
        glDeleteProgram(shaderProgram);
        glDeleteProgram(causticsProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    const GLint modelLocation = glGetUniformLocation(shaderProgram, "model");
    const GLint viewLocation = glGetUniformLocation(shaderProgram, "view");
    const GLint projectionLocation = glGetUniformLocation(shaderProgram, "projection");
    const GLint timeLocation = glGetUniformLocation(shaderProgram, "time");
    const GLint normalMatrixLocation = glGetUniformLocation(shaderProgram, "normalMatrix");
    const GLint lightDirectionLocation = glGetUniformLocation(shaderProgram, "lightDirection");
    const GLint cameraPositionLocation = glGetUniformLocation(shaderProgram, "cameraPosition");
    const GLint waveAmplitudeLocation = glGetUniformLocation(shaderProgram, "waveAmplitude");
    const GLint surfaceColorLocation = glGetUniformLocation(shaderProgram, "surfaceColor");
    const GLint surfaceOpacityLocation = glGetUniformLocation(shaderProgram, "surfaceOpacity");
    const GLint specularStrengthLocation = glGetUniformLocation(shaderProgram, "specularStrength");
    const GLint causticStrengthLocation = glGetUniformLocation(shaderProgram, "causticStrength");
    const GLint causticsMapLocation = glGetUniformLocation(shaderProgram, "causticsMap");
    const GLint causticBoundsMinLocation = glGetUniformLocation(shaderProgram, "causticBoundsMin");
    const GLint causticBoundsMaxLocation = glGetUniformLocation(shaderProgram, "causticBoundsMax");
    if (modelLocation < 0 || viewLocation < 0 || projectionLocation < 0 || timeLocation < 0
        || normalMatrixLocation < 0 || lightDirectionLocation < 0 || cameraPositionLocation < 0
        || waveAmplitudeLocation < 0 || surfaceColorLocation < 0 || surfaceOpacityLocation < 0
        || specularStrengthLocation < 0 || causticStrengthLocation < 0 || causticsMapLocation < 0
        || causticBoundsMinLocation < 0 || causticBoundsMaxLocation < 0) {
        std::cerr << "Failed to find a required uniform in the shader program.\n";
        glDeleteProgram(shaderProgram);
        glDeleteProgram(causticsProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    const GLint causticModelLocation = glGetUniformLocation(causticsProgram, "model");
    const GLint causticNormalMatrixLocation = glGetUniformLocation(causticsProgram, "normalMatrix");
    const GLint causticTimeLocation = glGetUniformLocation(causticsProgram, "time");
    const GLint causticWaveAmplitudeLocation = glGetUniformLocation(causticsProgram, "waveAmplitude");
    const GLint causticLightDirectionLocation = glGetUniformLocation(causticsProgram, "lightDirection");
    const GLint refractiveIndexRatioLocation = glGetUniformLocation(
        causticsProgram,
        "refractiveIndexRatio"
    );
    const GLint receiverHeightLocation = glGetUniformLocation(causticsProgram, "receiverHeight");
    const GLint causticsBoundsMinLocation = glGetUniformLocation(
        causticsProgram,
        "causticBoundsMin"
    );
    const GLint causticsBoundsMaxLocation = glGetUniformLocation(
        causticsProgram,
        "causticBoundsMax"
    );
    const GLint surfaceCellAreaLocation = glGetUniformLocation(causticsProgram, "surfaceCellArea");
    const GLint pointSizeLocation = glGetUniformLocation(causticsProgram, "pointSize");
    const GLint lightIntensityLocation = glGetUniformLocation(causticsProgram, "lightIntensity");
    const GLint texelWorldAreaLocation = glGetUniformLocation(causticsProgram, "texelWorldArea");
    if (causticModelLocation < 0 || causticNormalMatrixLocation < 0 || causticTimeLocation < 0
        || causticWaveAmplitudeLocation < 0 || causticLightDirectionLocation < 0
        || refractiveIndexRatioLocation < 0 || receiverHeightLocation < 0
        || causticsBoundsMinLocation < 0 || causticsBoundsMaxLocation < 0
        || surfaceCellAreaLocation < 0 || pointSizeLocation < 0 || lightIntensityLocation < 0
        || texelWorldAreaLocation < 0) {
        std::cerr << "Failed to find a required caustics uniform in the shader program.\n";
        glDeleteProgram(shaderProgram);
        glDeleteProgram(causticsProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    GLuint causticsFramebuffer = 0;
    GLuint causticsTexture = 0;
    if (!createCausticsTarget(causticsFramebuffer, causticsTexture)) {
        glDeleteProgram(shaderProgram);
        glDeleteProgram(causticsProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    const GridMesh mesh = createFlatGrid();
    constexpr GLfloat floorVertices[] = {
        -waterGridHalfExtent, 0.0F, -waterGridHalfExtent,
        -waterGridHalfExtent, 0.0F,  waterGridHalfExtent,
         waterGridHalfExtent, 0.0F,  waterGridHalfExtent,
        -waterGridHalfExtent, 0.0F, -waterGridHalfExtent,
         waterGridHalfExtent, 0.0F,  waterGridHalfExtent,
         waterGridHalfExtent, 0.0F, -waterGridHalfExtent
    };

    const glm::mat4 waterModel(1.0F);
    const glm::mat3 waterNormalMatrix = glm::inverseTranspose(glm::mat3(waterModel));
    const glm::mat4 floorModel = glm::translate(
        glm::mat4(1.0F),
        glm::vec3(0.0F, floorHeight, 0.0F)
    );
    const glm::mat3 floorNormalMatrix = glm::inverseTranspose(glm::mat3(floorModel));
    const glm::vec3 cameraPosition(5.0F, 5.0F, 8.0F);
    const glm::vec3 lightDirection = glm::normalize(glm::vec3(-0.4F, 0.1F, 0.1F));
    const glm::vec2 causticBoundsMin(-waterGridHalfExtent);
    const glm::vec2 causticBoundsMax(waterGridHalfExtent);
    const float gridSpacing = (2.0F * waterGridHalfExtent) / waterGridSubdivisions;
    const glm::vec2 causticBoundsSize = causticBoundsMax - causticBoundsMin;
    const float texelWorldArea = causticBoundsSize.x * causticBoundsSize.y
        / static_cast<float>(causticMapResolution * causticMapResolution);
    const float pointSize = gridSpacing * causticMapResolution
        / (causticBoundsMax.x - causticBoundsMin.x) * 1.2F;
    const glm::mat4 view = glm::lookAt(
        cameraPosition,
        glm::vec3(0.0F, -0.65F, 0.0F),
        glm::vec3(0.0F, 1.0F, 0.0F)
    );

    GLuint vertexArray = 0;
    GLuint vertexBuffer = 0;
    GLuint indexBuffer = 0;
    GLuint floorVertexArray = 0;
    GLuint floorVertexBuffer = 0;
    glGenVertexArrays(1, &vertexArray);
    glGenBuffers(1, &vertexBuffer);
    glGenBuffers(1, &indexBuffer);
    glGenVertexArrays(1, &floorVertexArray);
    glGenBuffers(1, &floorVertexBuffer);

    glBindVertexArray(vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(GLfloat)),
        mesh.vertices.data(),
        GL_STATIC_DRAW
    );
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(GLuint)),
        mesh.indices.data(),
        GL_STATIC_DRAW
    );
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glBindVertexArray(floorVertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, floorVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(floorVertices), floorVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glUseProgram(shaderProgram);
    glUniform1i(causticsMapLocation, 0);
    glUniform2fv(causticBoundsMinLocation, 1, glm::value_ptr(causticBoundsMin));
    glUniform2fv(causticBoundsMaxLocation, 1, glm::value_ptr(causticBoundsMax));

    glUseProgram(causticsProgram);
    glUniformMatrix4fv(causticModelLocation, 1, GL_FALSE, glm::value_ptr(waterModel));
    glUniformMatrix3fv(
        causticNormalMatrixLocation,
        1,
        GL_FALSE,
        glm::value_ptr(waterNormalMatrix)
    );
    glUniform3fv(causticLightDirectionLocation, 1, glm::value_ptr(lightDirection));
    glUniform1f(refractiveIndexRatioLocation, 1.0F / 1.333F);
    glUniform1f(receiverHeightLocation, floorHeight);
    glUniform2fv(causticsBoundsMinLocation, 1, glm::value_ptr(causticBoundsMin));
    glUniform2fv(causticsBoundsMaxLocation, 1, glm::value_ptr(causticBoundsMax));
    glUniform1f(surfaceCellAreaLocation, gridSpacing * gridSpacing);
    glUniform1f(pointSizeLocation, pointSize);
    glUniform1f(lightIntensityLocation, 1.0F);
    glUniform1f(texelWorldAreaLocation, texelWorldArea);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(viewLocation, 1, GL_FALSE, glm::value_ptr(view));
    glUniform3fv(lightDirectionLocation, 1, glm::value_ptr(lightDirection));
    glUniform3fv(cameraPositionLocation, 1, glm::value_ptr(cameraPosition));

    while (glfwWindowShouldClose(window) == GLFW_FALSE) {
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        const float currentTime = static_cast<float>(glfwGetTime());

        glBindFramebuffer(GL_FRAMEBUFFER, causticsFramebuffer);
        glViewport(0, 0, causticMapResolution, causticMapResolution);
        glDisable(GL_DEPTH_TEST);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        const GLfloat clearCaustics[] = {0.0F};
        glClearBufferfv(GL_COLOR, 0, clearCaustics);

        glUseProgram(causticsProgram);
        glUniform1f(causticTimeLocation, currentTime);
        glUniform1f(causticWaveAmplitudeLocation, waterWaveAmplitude);
        glBindVertexArray(vertexArray);
        glDrawArrays(
            GL_POINTS,
            0,
            static_cast<GLsizei>(mesh.vertices.size() / 3)
        );

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glEnable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(shaderProgram);

        if (framebufferWidth > 0 && framebufferHeight > 0) {
            const float aspectRatio = static_cast<float>(framebufferWidth)
                / static_cast<float>(framebufferHeight);
            const glm::mat4 projection = glm::perspective(
                glm::radians(45.0F),
                aspectRatio,
                0.1F,
                100.0F
            );
            glUniformMatrix4fv(projectionLocation, 1, GL_FALSE, glm::value_ptr(projection));
        }

        glClearColor(0.02F, 0.04F, 0.08F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUniform1f(timeLocation, currentTime);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, causticsTexture);

        glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(floorModel));
        glUniformMatrix3fv(normalMatrixLocation, 1, GL_FALSE, glm::value_ptr(floorNormalMatrix));
        glUniform1f(waveAmplitudeLocation, 0.0F);
        glUniform3f(surfaceColorLocation, 0.22F, 0.25F, 0.27F);
        glUniform1f(surfaceOpacityLocation, 1.0F);
        glUniform1f(specularStrengthLocation, 0.05F);
        glUniform1f(causticStrengthLocation, 1.0F);
        glBindVertexArray(floorVertexArray);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(waterModel));
        glUniformMatrix3fv(normalMatrixLocation, 1, GL_FALSE, glm::value_ptr(waterNormalMatrix));
        glUniform1f(waveAmplitudeLocation, waterWaveAmplitude);
        glUniform3f(surfaceColorLocation, 0.08F, 0.52F, 0.68F);
        glUniform1f(surfaceOpacityLocation, 0.72F);
        glUniform1f(specularStrengthLocation, 0.45F);
        glUniform1f(causticStrengthLocation, 0.0F);
        glBindVertexArray(vertexArray);
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(mesh.indices.size()),
            GL_UNSIGNED_INT,
            nullptr
        );

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    const GLuint buffers[] = {vertexBuffer, indexBuffer};
    glDeleteBuffers(2, buffers);
    glDeleteBuffers(1, &floorVertexBuffer);
    const GLuint vertexArrays[] = {vertexArray, floorVertexArray};
    glDeleteVertexArrays(2, vertexArrays);
    glDeleteFramebuffers(1, &causticsFramebuffer);
    glDeleteTextures(1, &causticsTexture);
    glDeleteProgram(shaderProgram);
    glDeleteProgram(causticsProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
