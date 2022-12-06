#include <iostream>             // cout, cerr
#include <cstdlib>              // EXIT_FAILURE
#include <GL/glew.h>            // GLEW library
#include <GLFW/glfw3.h>         // GLFW library
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>      // Image loading Utility functions

// GLM Math Header inclusions
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnOpengl/camera.h> // Camera class

using namespace std; // Standard namespace

/*Shader program Macro*/
#ifndef GLSL
#define GLSL(Version, Source) "#version " #Version " core \n" #Source
#endif

// Unnamed namespace
namespace
{
    const char* const WINDOW_TITLE = "Michael Kennedy"; // Macro for window title

    // Variables for window width and height
    const int WINDOW_WIDTH = 800;
    const int WINDOW_HEIGHT = 600;

    // Stores the GL data relative to a given mesh
    struct GLMesh
    {
        GLuint vao;         // Handle for the vertex array object
        GLuint vbos[2];     // Handles for the vertex buffer objects
        GLuint nIndices;    // Number of indices of the mesh
    };

    // Main GLFW window
    GLFWwindow* gWindow = nullptr;
    // Triangle mesh data
    GLMesh gMesh;
    // Texture ID
    GLuint textureMonitorId;
    // Shader programs
    GLuint gProgramId;
    GLuint textureProgramId;

    // Camera
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

    // Variables used to change attributes of camera
    bool firstMouse = true;
    float yaw = -90.0f;	// yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right so we initially rotate a bit to the left.
    float pitch = 0.0f;
    float lastX = 800.0f / 2.0;
    float lastY = 600.0 / 2.0;
    float sensitivity = 0.01f;

    // Projection
    bool ortho = false;
    glm::mat4 projection;

    // Timing
    float gDeltaTime = 0.0f; // time between current frame and last frame
    float gLastFrame = 0.0f;
}

/* User-defined Function prototypes to:
 * initialize the program, set the window size,
 * redraw graphics on the window when resized,
 * and render graphics on the screen
 */
bool UInitialize(int, char* [], GLFWwindow** window);
void UResizeWindow(GLFWwindow* window, int width, int height);
void UProcessInput(GLFWwindow* window);
void UMousePositionCallback(GLFWwindow* window, double xpos, double ypos);
void UMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void UMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void UCreateMesh(GLMesh& mesh);
void UDestroyMesh(GLMesh& mesh);
bool UCreateTexture(const char* filename, GLuint& textureId);
void UDestroyTexture(GLuint textureId);
void URender();
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId);
void UDestroyShaderProgram(GLuint programId);


/* Vertex Shader Source Code*/
const GLchar* vertexShaderSource = GLSL(440,
    layout(location = 0) in vec3 position; // Vertex data from Vertex Attrib Pointer 0
    layout(location = 1) in vec4 color;  // Color data from Vertex Attrib Pointer 1
    layout(location = 2) in vec2 textureCoordinate;

    out vec2 vertexTextureCoordinate;
    out vec4 vertexColor; // variable to transfer color data to the fragment shader

    //Global variables for the  transform matrices
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0f); // transforms vertices to clip coordinates
    vertexColor = color; // references incoming color data
    vertexTextureCoordinate = textureCoordinate;
}
);


/* Fragment Shader Source Code*/
const GLchar* fragmentShaderSource = GLSL(440,
    in vec4 vertexColor; // Variable to hold incoming color data from vertex shader

    out vec4 fragmentColor;


void main()
{
    fragmentColor = vec4(vertexColor);
}
);

/* Vertex Shader Source Code*/
const GLchar* textureVertexShaderSource = GLSL(440,
    layout(location = 0) in vec3 position;
    layout(location = 2) in vec2 textureCoordinate;

    out vec2 vertexTextureCoordinate;

void main()
{
    gl_Position = vec4(position.x, position.y, position.z, 1.0);
    vertexTextureCoordinate = textureCoordinate;
}
);

/* Fragment Shader Source Code*/
const GLchar* textureFragmentShaderSource = GLSL(440,
    in vec2 vertexTextureCoordinate;

    out vec4 fragmentColor;

    uniform sampler2D uTextureBase;

void main()
{
    fragmentColor = texture(uTextureBase, vertexTextureCoordinate);
}
);

// Images are loaded with Y axis going down, but OpenGL's Y axis goes up, so let's flip it
void flipImageVertically(unsigned char* image, int width, int height, int channels)
{
    for (int j = 0; j < height / 2; ++j)
    {
        int index1 = j * width * channels;
        int index2 = (height - 1 - j) * width * channels;

        for (int i = width * channels; i > 0; --i)
        {
            unsigned char tmp = image[index1];
            image[index1] = image[index2];
            image[index2] = tmp;
            ++index1;
            ++index2;
        }
    }
}

int main(int argc, char* argv[])
{
    if (!UInitialize(argc, argv, &gWindow))
        return EXIT_FAILURE;

    // Create the mesh
    UCreateMesh(gMesh); // Calls the function to create the Vertex Buffer Object

    // Create the shader program
    if (!UCreateShaderProgram(vertexShaderSource, fragmentShaderSource, gProgramId))
        return EXIT_FAILURE;
    
    // Create the shader program
    if (!UCreateShaderProgram(textureVertexShaderSource, textureFragmentShaderSource, textureProgramId))
        return EXIT_FAILURE;

    // Load textures
    const char* texFilename = "./milestone.jpg";
    if (!UCreateTexture(texFilename, textureMonitorId))
    {
        cout << "Failed to load texture " << texFilename << endl;
        return EXIT_FAILURE;
    }

    // tell opengl for each sampler to which texture unit it belongs to (only has to be done once)
    glUseProgram(textureProgramId);
    // We set the texture as texture unit 0
    glUniform1i(glGetUniformLocation(textureProgramId, "uTextureBase"), 0);

    // Sets the background color of the window to black (it will be implicitely used by glClear)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(gWindow))
    {
        // per-frame timing
        // --------------------
        float currentFrame = glfwGetTime();
        gDeltaTime = currentFrame - gLastFrame;
        gLastFrame = currentFrame;

        // input
        // -----
        UProcessInput(gWindow);

        // Render this frame
        URender();

        glfwPollEvents();
    }

    // Release mesh data
    UDestroyMesh(gMesh);

    // Release texture
    UDestroyTexture(textureMonitorId);

    // Release shader programs
    UDestroyShaderProgram(gProgramId);
    UDestroyShaderProgram(textureProgramId);

    exit(EXIT_SUCCESS); // Terminates the program successfully
}


// Initialize GLFW, GLEW, and create a window
bool UInitialize(int argc, char* argv[], GLFWwindow** window)
{
    // GLFW: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // GLFW: window creation
    // ---------------------
    * window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (*window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(*window);
    glfwSetFramebufferSizeCallback(*window, UResizeWindow);
    glfwSetCursorPosCallback(*window, UMousePositionCallback);
    glfwSetScrollCallback(*window, UMouseScrollCallback);
    glfwSetMouseButtonCallback(*window, UMouseButtonCallback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // GLEW: initialize
    // ----------------
    // Note: if using GLEW version 1.13 or earlier
    glewExperimental = GL_TRUE;
    GLenum GlewInitResult = glewInit();

    if (GLEW_OK != GlewInitResult)
    {
        std::cerr << glewGetErrorString(GlewInitResult) << std::endl;
        return false;
    }

    // Displays GPU OpenGL version
    cout << "INFO: OpenGL Version: " << glGetString(GL_VERSION) << endl;

    return true;
}


// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
void UProcessInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float cameraSpeed = static_cast<float>(2.5 * gDeltaTime) * (sensitivity * 100);

    // Capturing key presses to update the position of the camera
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
        ortho = !ortho;

}


// glfw: whenever the window size changed (by OS or user resize) this callback function executes
void UResizeWindow(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void UMousePositionCallback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    // Update camera position based on speed
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // Controlling pitch disallowing the camera from flipping
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    // Capturing the direction the camera is facing
    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(direction);
}


// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void UMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    // As the mouse scrolls the sensitivity is increased or descrease
    // NOTE: This works to a degree but has a flaw in that there seems to be a need for bounds but not sure
    sensitivity -= (float)yoffset / 100;
}

// glfw: handle mouse button events
// --------------------------------
void UMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    // Handle mouse button clicks
    // NOTE: Not used for this particular assignment but is left for future reference
    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT:
    {
        if (action == GLFW_PRESS)
            cout << "Left mouse button pressed" << endl;
        else
            cout << "Left mouse button released" << endl;
    }
    break;

    case GLFW_MOUSE_BUTTON_MIDDLE:
    {
        if (action == GLFW_PRESS)
            cout << "Middle mouse button pressed" << endl;
        else
            cout << "Middle mouse button released" << endl;
    }
    break;

    case GLFW_MOUSE_BUTTON_RIGHT:
    {
        if (action == GLFW_PRESS)
            cout << "Right mouse button pressed" << endl;
        else
            cout << "Right mouse button released" << endl;
    }
    break;

    default:
        cout << "Unhandled mouse button event" << endl;
        break;
    }
}


// Functioned called to render a frame
void URender()
{
    // Enable z-depth
    glEnable(GL_DEPTH_TEST);

    // Clear the frame and z buffers
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 1. Scales the object by 2
    glm::mat4 scale = glm::scale(glm::vec3(2.0f, 2.0f, 2.0f));
    // 2. Rotates shape first on the x axis to position and then on a continual rotation on y
    glm::mat4 rotation = glm::rotate(1.55f, glm::vec3(1.0, 0.0f, 0.0f));
    // 3. Place object at the origin
    glm::mat4 translation = glm::translate(glm::vec3(10.0f, 0.0f, -30.0f));

    // Model matrix: transformations are applied right-to-left order
    glm::mat4 model = translation * rotation * scale;

    if (ortho) {
        // Creates an ortho projection
        projection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, -100.0f, 100.0f);
    }
    else {
        // Creates a perspective projection
        projection = glm::perspective(glm::radians(45.0f), (GLfloat)WINDOW_WIDTH / (GLfloat)WINDOW_HEIGHT, 0.1f, 100.0f);
    }

    // camera/view transformation
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

    // Set the shader to be used
    glUseProgram(gProgramId);

    // Retrieves and passes transform matrices to the Shader program
    GLint modelLoc = glGetUniformLocation(gProgramId, "model");
    GLint viewLoc = glGetUniformLocation(gProgramId, "view");
    GLint projLoc = glGetUniformLocation(gProgramId, "projection");

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(gMesh.vao);

    // Draws the triangles
    glDrawElements(GL_TRIANGLES, (34 * 3), GL_UNSIGNED_SHORT, NULL); // Draws the triangle

    //glBindVertexArray(0);

    //----------------
    glUseProgram(textureProgramId);

    // Reference matrix uniforms from the Lamp Shader program
    modelLoc = glGetUniformLocation(textureProgramId, "model");
    viewLoc = glGetUniformLocation(textureProgramId, "view");
    projLoc = glGetUniformLocation(textureProgramId, "projection");

    // Pass matrix data to the Lamp Shader program's matrix uniforms
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(gMesh.vao);

    glDrawElements(GL_TRIANGLES, (2 * 3), GL_UNSIGNED_SHORT, (void *)(34 * 6));

    // Deactivate the Vertex Array Object and shader program
    glBindVertexArray(0);
    glUseProgram(0);

    // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
    glfwSwapBuffers(gWindow);    // Flips the the back buffer with the front buffer every frame.
}


// Implements the UCreateMesh function
void UCreateMesh(GLMesh& mesh)
{
    // Position and Color data
    GLfloat verts[] = {
        // Vertex Positions   // Colors (r,g,b,a)                   // Texture coordinates
        // Oil diffuser
         0.5f, -2.8f,  0.0f,  0.7f, 0.7f, 0.7f, 1.0f,               0.0, 0.0,
         0.5f, -3.8f,  0.0f,  0.7f, 0.7f, 0.7f, 1.0f,               0.0, 0.0,
        -0.5f, -3.8f,  0.0f,  0.7f, 0.7f, 0.7f, 1.0f,               0.0, 0.0,
        -0.5f, -2.8f,  0.0f,  0.7f, 0.7f, 0.7f, 1.0f,               0.0, 0.0,
         0.0f, -3.3f, -1.0f,  0.7f, 0.7f, 0.7f, 1.0f,               0.0, 0.0,
         0.5f, -2.8f,  1.5f,  0.7f, 0.7f, 0.7f, 1.0f,               0.0, 0.0,
         0.5f, -3.8f,  1.5f,  0.7f, 0.7f, 0.7f, 1.0f,               0.0, 0.0,
        -0.5f, -3.8f,  1.5f,  0.7f, 0.7f, 0.7f, 1.0f,               0.0, 0.0,
        -0.5f, -2.8f,  1.5f,  0.7f, 0.7f, 0.7f, 1.0f,               0.0, 0.0,

        // Desk
         0.6f,  0.6f,  1.5f,  0.5859375f, 0.29296875f, 0.0f, 1.0f,  0.0, 0.0,
         0.6f, -4.0f,  1.5f,  0.5859375f, 0.29296875f, 0.0f, 1.0f,  0.0, 0.0,
       -10.0f, -4.0f,  1.5f,  0.5859375f, 0.29296875f, 0.0f, 1.0f,  0.0, 0.0,
       -10.0f,  0.6f,  1.5f,  0.5859375f, 0.29296875f, 0.0f, 1.0f,  0.0, 0.0,

         0.6f,  0.6f,  2.0f,  0.5859375f, 0.29296875f, 0.0f, 1.0f,  0.0, 0.0,
         0.6f, -4.0f,  2.0f,  0.5859375f, 0.29296875f, 0.0f, 1.0f,  0.0, 0.0,
       -10.0f, -4.0f,  2.0f,  0.5859375f, 0.29296875f, 0.0f, 1.0f,  0.0, 0.0,
       -10.0f,  0.6f,  2.0f,  0.5859375f, 0.29296875f, 0.0f, 1.0f,  0.0, 0.0,

       // Monitor
        -2.0f, -3.5f,  1.2f,  1.0f, 1.0f, 1.0f, 1.0f,               0.0, 1.0,
        -8.0f, -3.5f,  1.2f,  1.0f, 1.0f, 1.0f, 1.0f,               1.0, 1.0,
        -2.0f, -3.5f, -3.0f,  1.0f, 1.0f, 1.0f, 1.0f,               0.0, 0.0,
        -8.0f, -3.5f, -3.0f,  1.0f, 1.0f, 1.0f, 1.0f,               1.0, 0.0,

        -2.0f, -3.8f,  1.2f,  1.0f, 1.0f, 1.0f, 1.0f,               0.0, 1.0,
        -8.0f, -3.8f,  1.2f,  1.0f, 1.0f, 1.0f, 1.0f,               1.0, 1.0,
        -2.0f, -3.8f, -3.0f,  1.0f, 1.0f, 1.0f, 1.0f,               0.0, 0.0,
        -8.0f, -3.8f, -3.0f,  1.0f, 1.0f, 1.0f, 1.0f,               1.0, 0.0,

    };

    // Index data to share position data
    GLushort indices[] = {
        // Oil diffuser tip
        1, 0, 4,
        1, 2, 4,
        2, 3, 4,
        3, 0, 4,

        // Oil diffuser sides
        0, 5, 6,
        0, 1, 6,
        1, 6, 7,
        2, 7, 1,
        0, 5, 3,
        3, 8, 5,
        2, 7, 3,
        3, 8, 7,

        // Oil diffuser bottom
        5, 6, 8,
        6, 7, 8,

        // Desk surface
        9, 10, 11,
        11, 12, 9,

        // Desk bottom
        13, 14, 15,
        15, 16, 13,

        // Desk sides
        9, 10, 13,
        13, 14, 10,
        10, 11, 14,
        14, 15, 11,
        11, 12, 15,
        15, 16, 12,
        12, 13, 16,
        16, 9, 13,

        // Monitor sides
        17, 18, 21,
        21, 22, 18,
        23, 20, 19,
        23, 24, 20,
        18, 22, 20,
        20, 24, 22,
        19, 23, 17,
        17, 21, 23,

        // Monitor front
        21, 22, 23,
        23, 24, 22,

        // Monitor rear
        17, 18, 19,
        19, 20, 18,
    };

    const GLuint floatsPerVertex = 3;
    const GLuint floatsPerColor = 4;
    const GLuint floatsPerTexture = 2;

    mesh.nIndices = sizeof(indices) / sizeof(indices[0]);

    glGenVertexArrays(1, &mesh.vao); // we can also generate multiple VAOs or buffers at the same time
    glBindVertexArray(mesh.vao);

    // Create 2 buffers: first one for the vertex data; second one for the indices
    glGenBuffers(2, mesh.vbos);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbos[0]); // Activates the buffer
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW); // Sends vertex or coordinate data to the GPU

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.vbos[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Strides between vertex coordinates is 9 (x, y, z, r, g, b, a, texture A, texture B). A tightly packed stride is 0.
    GLint stride = sizeof(float) * (floatsPerVertex + floatsPerColor + floatsPerTexture);// The number of floats before each

    // Create Vertex Attribute Pointers
    glVertexAttribPointer(0, floatsPerVertex, GL_FLOAT, GL_FALSE, stride, 0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, floatsPerColor, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * floatsPerVertex));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, floatsPerTexture, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * (floatsPerVertex + floatsPerColor)));
    glEnableVertexAttribArray(2);
}


void UDestroyMesh(GLMesh& mesh)
{
    glDeleteVertexArrays(1, &mesh.vao);
    glDeleteBuffers(2, mesh.vbos);
}

/*Generate and load the texture*/
bool UCreateTexture(const char* filename, GLuint& textureId)
{
    int width, height, channels;
    unsigned char* image = stbi_load(filename, &width, &height, &channels, 0);
    if (image)
    {
        flipImageVertically(image, width, height, channels);

        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);

        // set the texture wrapping parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        // set texture filtering parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (channels == 3)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        else if (channels == 4)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
        else
        {
            cout << "Not implemented to handle image with " << channels << " channels" << endl;
            return false;
        }

        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(image);
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture

        return true;
    }

    // Error loading the image
    return false;
}


void UDestroyTexture(GLuint textureId)
{
    glGenTextures(1, &textureId);
}

// Implements the UCreateShaders function
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId)
{
    // Compilation and linkage error reporting
    int success = 0;
    char infoLog[512];

    // Create a Shader program object.
    programId = glCreateProgram();

    // Create the vertex and fragment shader objects
    GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);

    // Retrive the shader source
    glShaderSource(vertexShaderId, 1, &vtxShaderSource, NULL);
    glShaderSource(fragmentShaderId, 1, &fragShaderSource, NULL);

    // Compile the vertex shader, and print compilation errors (if any)
    glCompileShader(vertexShaderId); // compile the vertex shader
    // check for shader compile errors
    glGetShaderiv(vertexShaderId, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShaderId, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;

        return false;
    }

    glCompileShader(fragmentShaderId); // compile the fragment shader
    // check for shader compile errors
    glGetShaderiv(fragmentShaderId, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShaderId, sizeof(infoLog), NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;

        return false;
    }

    // Attached compiled shaders to the shader program
    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);

    glLinkProgram(programId);   // links the shader program
    // check for linking errors
    glGetProgramiv(programId, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(programId, sizeof(infoLog), NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;

        return false;
    }

    glUseProgram(programId);    // Uses the shader program

    return true;
}


void UDestroyShaderProgram(GLuint programId)
{
    glDeleteProgram(programId);
}

