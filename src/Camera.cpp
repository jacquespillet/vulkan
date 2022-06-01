#include "Camera.h"
#include <algorithm>
#include <iostream>
void camera::RecalculateLookat() {
    worldPosition = target + sphericalPosition * distance;
    invModelMatrix = glm::lookAt(worldPosition, target, up);
    modelMatrix = glm::inverse(invModelMatrix);
}


void camera::mousePressEvent(int button) {
    if(Locked) return;
    std::cout << button << std::endl;
    if(button==0) {
        IsLeftMousePressed=true;
    } else if(button==1) {
        IsRightMousePressed=true;
    }
}

void camera::mouseReleaseEvent(int button) {
    if(Locked) return;
    
    IsLeftMousePressed=false;
    IsRightMousePressed=false;
    prevPos = glm::ivec2(-1, -1);
    Changed=true;
}

void camera::mouseMoveEvent(float x, float y) {
    if(Locked) return;

    glm::vec2 currentPos = glm::vec2(x, y);
    if(IsLeftMousePressed) {
        if(prevPos.x >0) {
            glm::vec2 diff = currentPos - prevPos;
            
            phi += (float)diff.x * 0.005f;
            theta -= (float)diff.y * 0.005f;
            
            phi = (phi>2*PI)? phi - 2.0f * PI : phi;
            phi = (phi<0) ? 2.0f * PI + phi : phi;

            theta = std::max(0.00001f, std::min(theta,(float)PI-0.00001f));

            sphericalPosition.x = sin(theta) * cos(phi);
            sphericalPosition.z = sin(theta) * sin(phi);                  
            sphericalPosition.y = cos(theta);
            RecalculateLookat();  
            Changed=true;
        }
    } else if (IsRightMousePressed) {
        if(prevPos.x >0) {
            glm::ivec2 diff = currentPos - prevPos;
            target -= (float)diff.x * 0.005 * distance * glm::vec3(modelMatrix[0]);
            target += (float)diff.y * 0.005 * distance *  glm::vec3(modelMatrix[1]);
            RecalculateLookat();
            Changed=true; 
        }
    }
    prevPos = currentPos;
}

void camera::Scroll(float offset){
    if(Locked) return;
    distance -= (float)offset * distance * 0.05f;
    RecalculateLookat();   
    Changed=true;
}

void camera::GetScreenRay(glm::vec2 ndc, float aspect, glm::vec3& rayOrig, glm::vec3& rayDir)
{	
    float degToRad = 0.01745329251f; 
	ndc.x = (ndc.x - 0.5f) * 2.0f;
	ndc.y = ((1.0f - ndc.y) - 0.5f) * 2.0f;
    float camLength = -1.0f / tan(fov / 2.0f * degToRad); 
    glm::vec4 rayDirection = glm::normalize(glm::vec4(ndc.x * aspect, ndc.y, camLength, 0));
    rayDir = glm::vec3(modelMatrix * rayDirection);
    rayOrig = glm::vec3(modelMatrix * glm::vec4(0, 0, 0, 1)); 

}