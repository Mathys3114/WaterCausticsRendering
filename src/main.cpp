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
    constexpr GLuint subdivisions = 50;
    constexpr float halfExtent = 2.5F;
    constexpr GLuint verticesPerSide = subdivisions + 1;
    constexpr float spacing = (2.0F * halfExtent) / subdivisions;

    GridMesh mesh;
    mesh.vertices.reserve(static_cast<std::size_t>(verticesPerSide) * verticesPerSide * 3);
    mesh.indices.reserve(static_cast<std::size_t>(subdivisions) * subdivisions * 6);

    for (GLuint row = 0; row < verticesPerSide; ++row) {
        for (GLuint column = 0; column < verticesPerSide; ++column) {
            mesh.vertices.push_back(-halfExtent + spacing * column);
            mesh.vertices.push_back(0.0F);
            mesh.vertices.push_back(-halfExtent + spacing * row);
        }
    }

    for (GLuint row = 0; row < subdivisions; ++row) {
        for (GLuint column = 0; column < subdivisions; ++column) {
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
    const std::string shaderDirectory = WATER_CAUSTICS_SHADER_DIR;
    if (!loadShaderSource(shaderDirectory + "/quad.vert", vertexSource)
        || !loadShaderSource(shaderDirectory + "/quad.frag", fragmentSource)) {
        return EXIT_FAILURE;
    }

    if (glfwInit() != GLFW_TRUE) {
        std::cerr << "Failed to initialize GLFW.\n";
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

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
    if (shaderProgram == 0) {
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
    if (modelLocation < 0 || viewLocation < 0 || projectionLocation < 0 || timeLocation < 0
        || normalMatrixLocation < 0 || lightDirectionLocation < 0 || cameraPositionLocation < 0) {
        std::cerr << "Failed to find a required uniform in the shader program.\n";
        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    const GridMesh mesh = createFlatGrid();

    const glm::mat4 model(1.0F);
    const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(model));
    const glm::vec3 cameraPosition(0.0F, 4.0F, 6.0F);
    const glm::vec3 lightDirection = glm::normalize(glm::vec3(-0.4F, 0.3F, 0.3F));
    const glm::mat4 view = glm::lookAt(
        cameraPosition,
        glm::vec3(0.0F, 0.0F, 0.0F),
        glm::vec3(0.0F, 1.0F, 0.0F)
    );

    GLuint vertexArray = 0;
    GLuint vertexBuffer = 0;
    GLuint indexBuffer = 0;
    glGenVertexArrays(1, &vertexArray);
    glGenBuffers(1, &vertexBuffer);
    glGenBuffers(1, &indexBuffer);
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

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(viewLocation, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix3fv(normalMatrixLocation, 1, GL_FALSE, glm::value_ptr(normalMatrix));
    glUniform3fv(lightDirectionLocation, 1, glm::value_ptr(lightDirection));
    glUniform3fv(cameraPositionLocation, 1, glm::value_ptr(cameraPosition));

    while (glfwWindowShouldClose(window) == GLFW_FALSE) {
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
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
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glUniform1f(timeLocation, static_cast<float>(glfwGetTime()));
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
    glDeleteVertexArrays(1, &vertexArray);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
