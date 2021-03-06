#pragma once
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext.hpp>

#define PI 3.14159265359f
#define TWO_PI 6.28318530717958647692529f

class camera {
public:
    camera(glm::vec3 position, float fov=80.0f, float nearPlane=0.01f, float farPlane=150.0f, float aspectRatio=1.7778f) : fov(fov), nearPlane(nearPlane), farPlane(farPlane), aspectRatio(aspectRatio), sphericalPosition(glm::vec3(0, 0, 1)), target(position), up(glm::vec3(0,1,0)){ 
        worldPosition = target + sphericalPosition * distance;
        theta =  atan2(sqrt(sphericalPosition.x * sphericalPosition.x  + sphericalPosition.z * sphericalPosition.z), sphericalPosition.y);
        phi = atan2(sphericalPosition.z, sphericalPosition.x); 

        RecalculateProjectionMatrix();
        RecalculateLookat();
    }
    camera() : fov(80.0f), nearPlane(0.01f), farPlane(300.0f), aspectRatio(1.0f), sphericalPosition(glm::vec3(0, 0, 1)), target(glm::vec3(0,0,0)), up(glm::vec3(0,1,0)){
        worldPosition = target + sphericalPosition * distance;
        theta =  atan2(sqrt(sphericalPosition.x * sphericalPosition.x  + sphericalPosition.z * sphericalPosition.z), sphericalPosition.y);
        phi = atan2(sphericalPosition.z, sphericalPosition.x); 
                
        RecalculateProjectionMatrix(); 
        RecalculateLookat();
    }

    void SetFov(float _fov) {this->fov = _fov; RecalculateProjectionMatrix();}
    void SetNearPlane(float _nearPlane) {this->nearPlane = _nearPlane; RecalculateProjectionMatrix();}
    void SetFarPlane(float _farPlane) {this->farPlane = _farPlane; RecalculateProjectionMatrix();}
    void SetAspectRatio(float _aspectRatio) {this->aspectRatio = _aspectRatio; RecalculateProjectionMatrix();}
    void SetDistance(float _distance) {this->distance = _distance; RecalculateLookat();}
    void SetSphericalPosition(glm::vec3 _position) {
        this->sphericalPosition = _position; 
        theta =  atan2(sqrt(sphericalPosition.x * sphericalPosition.x  + sphericalPosition.z * sphericalPosition.z), sphericalPosition.y);
        phi = atan2(sphericalPosition.z, sphericalPosition.x); 
        RecalculateLookat();
    }
    void RecalculateProjectionMatrix() {
        projectionMatrix = glm::perspectiveRH(glm::radians(fov), aspectRatio, nearPlane, farPlane);
        projectionMatrix[1][1] *= -1;

        InverseProjectionMatrix = glm::inverse(projectionMatrix);
    }

    glm::mat4 GetProjectionMatrix() const{return projectionMatrix;}
    glm::mat4 GetInverseProjectionMatrix() const{return InverseProjectionMatrix;}
    glm::mat4 GetViewMatrix() const{return invModelMatrix;}
    glm::mat4 GetModelMatrix() const{return modelMatrix;}
    float GetFov() const{return fov;}

    void RecalculateLookat();

    void mousePressEvent(int button);
    void mouseReleaseEvent(int button);
    void mouseMoveEvent(float x, float y);
    void Scroll(float offset);
    
    void GetScreenRay(glm::vec2 ndc, float aspect, glm::vec3& rayOrig, glm::vec3& rayDir);
    glm::vec3 worldPosition;

    bool Locked=false;
    bool Changed=false;

    float GetNearPlane() {return nearPlane;}
    float GetFarPlane() {return farPlane;}

    void Lock();
    void Unlock();
    
    glm::mat4 PrevInvModelMatrix;
    glm::mat4 invModelMatrix;
private:    
    float fov;
    float nearPlane, farPlane;
    float aspectRatio;
    
    glm::vec3 sphericalPosition;
    glm::vec3 target;
    glm::vec3 up;
    float phi=0;
    float theta=0;
    float distance = 5;

    glm::mat4 projectionMatrix;
    glm::mat4 InverseProjectionMatrix;

    bool IsLeftMousePressed=false;
    bool IsRightMousePressed=false;
    glm::vec2 prevPos = glm::vec2(-1.f, -1.f);

    glm::mat4 modelMatrix;

    

};