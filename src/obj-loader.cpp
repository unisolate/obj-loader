#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

#include <GL/glew.h>

#include <glfw3.h>
GLFWwindow* window;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#define W_WIDTH 1024
#define W_HEIGHT 768

mat4 v_mat;
mat4 p_mat;

mat4 getViewMatrix() { return v_mat; }
mat4 getProjectionMatrix() { return p_mat; }

vec3 position = vec3(0, 0, 5);  // +Z
float h_angle = 3.14f;          // -Z
float v_angle = 0.0f;
float init_fov = 45.0f;

float speed = 10.0f;
float mouse_speed = 0.005f;

void computeMatricesFromInputs() {
    static double last_t = glfwGetTime();  // called only once
    double curr_t = glfwGetTime();
    float delta_t = float(curr_t - last_t);

    // double xpos, ypos;
    // glfwGetCursorPos(window, &xpos, &ypos);               // get mouse pos
    // glfwSetCursorPos(window, W_WIDTH / 2, W_HEIGHT / 2);  // reset pos

    // get new orientation
    // h_angle += mouse_speed * float(W_WIDTH / 2 - xpos);
    // v_angle += mouse_speed * float(W_HEIGHT / 2 - ypos);

    // convert spherical coordinates to cartesian coordinates
    vec3 direction(cos(v_angle) * sin(h_angle), sin(v_angle),
                   cos(v_angle) * cos(h_angle));

    // vec3 right =
    //     vec3(sin(h_angle - 3.14f / 2.0f), 0, cos(h_angle - 3.14f / 2.0f));
    vec3 right =
        vec3(sin(h_angle - 3.14f / 2.0f), 0, cos(h_angle - 3.14f / 2.0f));
    vec3 up = cross(right, direction);

    // move forward
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        position += direction * delta_t * speed;
    }
    // move backward
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        position -= direction * delta_t * speed;
    }
    // rotate right
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        position += right * delta_t * speed;
    }
    // rotate left
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        position -= right * delta_t * speed;
    }

    // TODO: change fov using mouse wheel
    float fov = init_fov;  // - 5 * glfwGetMouseWheel();

    // projection matrix: 45° field of view, 4:3 ratio
    // display range: 0.1 unit <-> 100 units
    p_mat = perspective(fov, 4.0f / 3.0f, 0.1f, 100.0f);
    // camera matrix
    v_mat = lookAt(position,              // camera pos
                   position + direction,  // look-at direction
                   up                     // down: (0, -1, 0)
                   );

    last_t = curr_t;
}

#define FOURCC_DXT1 0x31545844  // ASCII of "DXT1"
#define FOURCC_DXT3 0x33545844
#define FOURCC_DXT5 0x35545844

GLuint loadDDS(const char* imagepath) {
    unsigned char header[124];

    FILE* fp;
    fp = fopen(imagepath, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open image.\n");
        return 0;
    }

    // check file type
    char filecode[4];
    fread(filecode, 1, 4, fp);
    if (strncmp(filecode, "DDS ", 4) != 0) {
        fclose(fp);
        return 0;
    }

    fread(&header, 124, 1, fp);  // get surface desc

    unsigned int height = *(unsigned int*)&(header[8]);
    unsigned int width = *(unsigned int*)&(header[12]);
    unsigned int linear_size = *(unsigned int*)&(header[16]);
    unsigned int mip_map_cnt = *(unsigned int*)&(header[24]);
    unsigned int four_cc = *(unsigned int*)&(header[80]);

    unsigned char* buffer;
    unsigned int bufsize;
    bufsize = mip_map_cnt > 1 ? linear_size * 2 : linear_size;
    buffer = (unsigned char*)malloc(bufsize * sizeof(unsigned char));
    fread(buffer, 1, bufsize, fp);
    fclose(fp);

    // unsigned int components = (four_cc == FOURCC_DXT1) ? 3 : 4;
    unsigned int format;
    switch (four_cc) {
        case FOURCC_DXT1:
            format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
            break;
        case FOURCC_DXT3:
            format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
            break;
        case FOURCC_DXT5:
            format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            break;
        default:
            free(buffer);
            return 0;
    }

    GLuint texture_id;
    glGenTextures(1, &texture_id);

    glBindTexture(GL_TEXTURE_2D, texture_id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    unsigned int blockSize =
        (format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16;
    unsigned int offset = 0;

    // load mipmaps
    for (unsigned int level = 0; level < mip_map_cnt && (width || height);
         ++level) {
        unsigned int size = ((width + 3) / 4) * ((height + 3) / 4) * blockSize;
        glCompressedTexImage2D(GL_TEXTURE_2D, level, format, width, height, 0,
                               size, buffer + offset);

        offset += size;
        width /= 2;
        height /= 2;

        if (width < 1) width = 1;
        if (height < 1) height = 1;
    }
    free(buffer);
    return texture_id;
}

GLuint LoadShaders(const char* vertex_file_path,
                   const char* fragment_file_path) {
    GLuint v_shader_id = glCreateShader(GL_VERTEX_SHADER);
    GLuint f_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

    string v_shader_code;
    ifstream v_shader_stream(vertex_file_path, ios::in);
    if (v_shader_stream.is_open()) {
        string line = "";
        while (getline(v_shader_stream, line)) {
            v_shader_code += "\n" + line;
        }
        v_shader_stream.close();
    } else {
        fprintf(stderr, "Failed to open shader.\n");
        return 0;
    }

    string f_shader_code;
    ifstream f_shader_stream(fragment_file_path, ios::in);
    if (f_shader_stream.is_open()) {
        string line = "";
        while (getline(f_shader_stream, line)) f_shader_code += "\n" + line;
        f_shader_stream.close();
    }

    GLint result = GL_FALSE;
    int info_log_len;

    char const* v_source_ptr = v_shader_code.c_str();
    glShaderSource(v_shader_id, 1, &v_source_ptr, NULL);
    glCompileShader(v_shader_id);

    glGetShaderiv(v_shader_id, GL_COMPILE_STATUS, &result);
    glGetShaderiv(v_shader_id, GL_INFO_LOG_LENGTH, &info_log_len);
    if (info_log_len > 0) {
        vector<char> v_shader_errmsg(info_log_len + 1);
        glGetShaderInfoLog(v_shader_id, info_log_len, NULL,
                           &v_shader_errmsg[0]);
        fprintf(stderr, "%s\n", &v_shader_errmsg[0]);
    }

    char const* f_source_ptr = f_shader_code.c_str();
    glShaderSource(f_shader_id, 1, &f_source_ptr, NULL);
    glCompileShader(f_shader_id);

    glGetShaderiv(f_shader_id, GL_COMPILE_STATUS, &result);
    glGetShaderiv(f_shader_id, GL_INFO_LOG_LENGTH, &info_log_len);
    if (info_log_len > 0) {
        vector<char> f_shader_errmsg(info_log_len + 1);
        glGetShaderInfoLog(f_shader_id, info_log_len, NULL,
                           &f_shader_errmsg[0]);
        fprintf(stderr, "%s\n", &f_shader_errmsg[0]);
    }

    GLuint prog_id = glCreateProgram();
    glAttachShader(prog_id, v_shader_id);
    glAttachShader(prog_id, f_shader_id);
    glLinkProgram(prog_id);

    glGetProgramiv(prog_id, GL_LINK_STATUS, &result);
    glGetProgramiv(prog_id, GL_INFO_LOG_LENGTH, &info_log_len);
    if (info_log_len > 0) {
        vector<char> prog_errmsg(info_log_len + 1);
        glGetProgramInfoLog(prog_id, info_log_len, NULL, &prog_errmsg[0]);
        fprintf(stderr, "%s\n", &prog_errmsg[0]);
    }

    glDetachShader(prog_id, v_shader_id);
    glDetachShader(prog_id, f_shader_id);

    glDeleteShader(v_shader_id);
    glDeleteShader(f_shader_id);

    return prog_id;
}

int loadOBJ(const char* path, vector<vec3>& out_vertices, vector<vec2>& out_uvs,
            vector<vec3>& out_normals) {
    vector<unsigned int> vertex_idx, uv_idx, normal_idx;
    vector<vec3> temp_vertices;
    vector<vec2> temp_uvs;
    vector<vec3> temp_normals;

    FILE* file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }

    char line[128];
    while (fscanf(file, "%s", line) != EOF) {
        if (strcmp(line, "v") == 0) {
            vec3 vertex;
            fscanf(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z);
            temp_vertices.push_back(vertex);
        } else if (strcmp(line, "vt") == 0) {
            vec2 uv;
            fscanf(file, "%f %f\n", &uv.x, &uv.y);
            uv.y = -uv.y;  // invert v coordinate for DDS texture
            temp_uvs.push_back(uv);
        } else if (strcmp(line, "vn") == 0) {
            vec3 normal;
            fscanf(file, "%f %f %f\n", &normal.x, &normal.y, &normal.z);
            temp_normals.push_back(normal);
        } else if (strcmp(line, "f") == 0) {
            int sz = (temp_uvs.size() ? 1 : 0) + (temp_normals.size() ? 1 : 0);
            unsigned int vertex_i, uv_i, normal_i;

            for (int i = 0; i < 3; ++i) {
                if (sz) {
                    fscanf(file, "%d/%d/%d", &vertex_i, &uv_i, &normal_i);
                    uv_idx.push_back(uv_i);
                    normal_idx.push_back(normal_i);
                } else {
                    fscanf(file, "%d", &vertex_i);
                }
                vertex_idx.push_back(vertex_i);
            }
        } else {
            char tmpbuffer[1000];
            fgets(tmpbuffer, 1000, file);
        }
    }

    int sz = (temp_uvs.size() ? 1 : 0) + (temp_normals.size() ? 1 : 0);
    for (unsigned int i = 0; i < vertex_idx.size(); i++) {
        unsigned int vertex_i = vertex_idx[i];
        vec3 vertex = temp_vertices[vertex_i - 1];
        out_vertices.push_back(vertex);

        if (sz) {
            unsigned int uv_i = uv_idx[i];
            unsigned int normal_i = normal_idx[i];

            vec2 uv = temp_uvs[uv_i - 1];
            vec3 normal = temp_normals[normal_i - 1];

            out_uvs.push_back(uv);
            out_normals.push_back(normal);
        }
    }

    return (temp_uvs.size() ? 1 : 0);
}

int main(void) {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW.\n");
        return -1;
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);  // OS X required
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // open window
    window = glfwCreateWindow(W_WIDTH, W_HEIGHT, "OBJ Loader", NULL, NULL);
    if (window == NULL) {
        fprintf(stderr, "Failed to open GLFW window.\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glewExperimental = true;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW.\n");
        glfwTerminate();
        return -1;
    }

    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    // glfwPollEvents();
    // glfwSetCursorPos(window, W_WIDTH / 2, W_HEIGHT / 2);

    // black background
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);

    GLuint v_array_id;
    glGenVertexArrays(1, &v_array_id);
    glBindVertexArray(v_array_id);

    GLuint prog_id = LoadShaders("StandardShading.vertexshader",
                                 "StandardShading.fragmentshader");

    GLuint mat_id = glGetUniformLocation(prog_id, "MVP");
    GLuint v_mat_id = glGetUniformLocation(prog_id, "V");
    GLuint m_mat_id = glGetUniformLocation(prog_id, "M");

    GLuint texture = loadDDS("uvmap.DDS");
    GLuint texture_id = glGetUniformLocation(prog_id, "myTextureSampler");

    // read obj file
    vector<vec3> vertices;
    vector<vec2> uvs;
    vector<vec3> normals;
    int res = loadOBJ("suzanne.obj", vertices, uvs, normals);
    if (res < 0) {
        fprintf(stderr, "Failed to parse obj file.\n");
        return -1;
    }

    GLuint vertexbuffer;
    glGenBuffers(1, &vertexbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vec3), &vertices[0],
                 GL_STATIC_DRAW);

    GLuint uvbuffer;
    if (res) {
        glGenBuffers(1, &uvbuffer);
        glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
        glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(vec2), &uvs[0],
                     GL_STATIC_DRAW);
    }

    GLuint normalbuffer;
    if (res) {
        glGenBuffers(1, &normalbuffer);
        glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
        glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(vec3),
                     &normals[0], GL_STATIC_DRAW);
    }

    glUseProgram(prog_id);
    GLuint light_id = glGetUniformLocation(prog_id, "LightPosition_worldspace");

    do {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(prog_id);

        computeMatricesFromInputs();
        mat4 p_mat = getProjectionMatrix();
        mat4 v_mat = getViewMatrix();
        mat4 m_mat = mat4(1.0);
        mat4 MVP = p_mat * v_mat * m_mat;

        glUniformMatrix4fv(mat_id, 1, GL_FALSE, &MVP[0][0]);
        glUniformMatrix4fv(m_mat_id, 1, GL_FALSE, &m_mat[0][0]);
        glUniformMatrix4fv(v_mat_id, 1, GL_FALSE, &v_mat[0][0]);

        vec3 lightPos = vec3(4, 4, 4);
        glUniform3f(light_id, lightPos.x, lightPos.y, lightPos.z);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(texture_id, 0);

        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
        glVertexAttribPointer(0,         // attribute
                              3,         // size
                              GL_FLOAT,  // type
                              GL_FALSE,  // normalized?
                              0,         // stride
                              (void*)0   // array buffer offset
                              );

        if (res) {
            glEnableVertexAttribArray(1);
            glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
            glVertexAttribPointer(1,         // attribute
                                  2,         // size
                                  GL_FLOAT,  // type
                                  GL_FALSE,  // normalized?
                                  0,         // stride
                                  (void*)0   // array buffer offset
                                  );

            glEnableVertexAttribArray(2);
            glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
            glVertexAttribPointer(2,         // attribute
                                  3,         // size
                                  GL_FLOAT,  // type
                                  GL_FALSE,  // normalized?
                                  0,         // stride
                                  (void*)0   // array buffer offset
                                  );
        }

        glDrawArrays(GL_TRIANGLES, 0, vertices.size());

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);

        glfwSwapBuffers(window);
        glfwPollEvents();

    } while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
             glfwWindowShouldClose(window) == 0);

    glDeleteBuffers(1, &vertexbuffer);
    if (res) {
        glDeleteBuffers(1, &uvbuffer);
        glDeleteBuffers(1, &normalbuffer);
    }
    glDeleteProgram(prog_id);
    glDeleteTextures(1, &texture_id);
    glDeleteVertexArrays(1, &v_array_id);

    glfwTerminate();

    return 0;
}
