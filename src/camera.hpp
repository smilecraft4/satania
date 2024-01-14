#pragma once

#define GLFW_INCLUDE_NONE
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:
    Camera(float distance = 2.0f, float elevation = 0.0f, float azimuth = 0.0f, glm::vec3 target = glm::vec3(0.0f))
    {
        _distance = distance;
        _elev = elevation;
        _azim = azimuth;
        _target = target;
    }

    void update(GLFWwindow *window, glm::vec2 mouse_delta)
    {
        glm::vec3 up = _orientation * glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 right = _orientation * glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 forward = _orientation * glm::vec3(0.0f, 0.0f, -1.0f);

        // Orbiting (LMB)
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            _azim += mouse_delta.x;
            _elev += mouse_delta.y;
        }

        // Panning (MMB)
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
        {
            _target += -mouse_delta.y * up * _pan_speed * (_distance / 5.0f);
            _target += mouse_delta.x * right * _pan_speed * (_distance / 5.0f);
        }

        // Zooming (RMB)
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
        {
            // Stop the camera to go beyond the target position
            if (mouse_delta.y > 0.0f)
            {
                if (_distance > 0.1f)
                {
                    _distance += -mouse_delta.y * _zoom_speed * _distance;
                }
            }
            else
            {
                _distance += -mouse_delta.y * _zoom_speed * _distance;
            }
        }

        // Thank you https://stackoverflow.com/a/34584536
        glm::mat4 pos_mat = glm::translate(glm::mat4(1.0f), _target);
        pos_mat = glm::translate(pos_mat, glm::vec3(0.0f, 0.0f, -1.0f) * _distance);

        glm::mat4 azim_mat = glm::translate(glm::mat4(1.0f), -_target);
        azim_mat = glm::rotate(azim_mat, _azim * _orbit_speed, glm::vec3(0.0f, 1.0f, 0.0f));
        azim_mat = glm::translate(azim_mat, _target);

        glm::mat4 elev_mat = glm::translate(glm::mat4(1.0f), -_target);
        elev_mat = glm::rotate(elev_mat, _elev * _orbit_speed, glm::vec3(1.0f, 0.0f, 0.0f));
        elev_mat = glm::translate(elev_mat, _target);

        _view = pos_mat * elev_mat * azim_mat;
        _orientation = glm::quat_cast(glm::transpose(glm::mat3(_view)));

        int width, height;
        glfwGetWindowSize(window, &width, &height);

        if (width != 0 && height != 0)
        {
            _projection = glm::perspective(45.0f, width / (float)height, _near, _far);
        }
        else
        {
            _projection = glm::mat4(1.0f);
        }
    }

    glm::mat4 _view;
    glm::mat4 _projection;

private:
    glm::vec3 _target;
    float _distance;
    float _azim;
    float _elev;

    glm::quat _orientation;

    const float _pan_speed = 0.01f;
    const float _orbit_speed = 0.005f;
    const float _zoom_speed = 0.005f;
    const float _fov = 45.0f;
    const float _near = 0.05f;
    const float _far = 5000.0f;
};