#pragma once
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/ext.hpp>


#define PI 3.14159265359f
#define TWO_PI 6.28318530717958647692529
#define RAD2DEG 57.2958f
#define DEG2RAD 0.0174533f

namespace util
{
    void DecomposeMatrix(glm::mat4 matrix, glm::vec3 *translation, glm::vec3 *rotation, glm::vec3 *scale)
    {
        glm::vec3 right(matrix[0][0], matrix[0][1], matrix[0][2]);
        glm::vec3 up(matrix[1][0], matrix[1][1], matrix[1][2]);
        glm::vec3 forward(matrix[2][0], matrix[2][1], matrix[2][2]);

        scale->x = glm::length(right);
        scale->y = glm::length(up);
        scale->z = glm::length(forward);

        glm::mat3 ortho = glm::orthonormalize(glm::mat3(matrix));

        rotation->x = RAD2DEG * atan2f(ortho[1][2], ortho[2][2]);
        rotation->y = RAD2DEG * atan2f(-ortho[0][2], sqrtf(ortho[1][2] * ortho[1][2] + ortho[2][2]* ortho[2][2]));
        rotation->z = RAD2DEG * atan2f(ortho[0][1], ortho[0][0]);

        translation->x = matrix[3][0];
        translation->y = matrix[3][1];
        translation->z = matrix[3][2];
    }
}
