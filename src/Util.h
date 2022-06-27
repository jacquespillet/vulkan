#pragma once
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/ext.hpp>


#define PI 3.14159265359f
#define TWO_PI 6.28318530717958647692529f
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
    
    glm::mat4 QuatToMat(glm::quat Quaternion)
    {
        glm::mat4 out;
        float x = Quaternion.x;
        float y = Quaternion.y;
        float z = Quaternion.z;
        float w = Quaternion.w;

        const float x2 = x + x;
        const float y2 = y + y;
        const float z2 = z + z;

        const float xx = x * x2;
        const float xy = x * y2;
        const float xz = x * z2;

        const float yy = y * y2;
        const float yz = y * z2;
        const float zz = z * z2;

        const float wx = w * x2;
        const float wy = w * y2;
        const float wz = w * z2;

        out[0][0] = 1.0f - (yy + zz);
        out[0][1] = xy + wz;
        out[0][2] = xz - wy;
        out[0][3] = 0.0f;

        out[1][0] = xy - wz;
        out[1][1] = 1.0f - (xx + zz);
        out[1][2] = yz + wx;
        out[1][3] = 0.0f;

        out[2][0] = xz + wy;
        out[2][1] = yz - wx;
        out[2][2] = 1.0f - (xx + yy);
        out[2][3] = 0.0f;

        out[3][0] = 0;
        out[3][1] = 0;
        out[3][2] = 0;
        out[3][3] = 1.0f;

        return out;        
    }
}
