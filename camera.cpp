#include "camera.h"

void CameraPositionerFirstPerson::update(double deltaSeconds, const glm::vec2& mousePos, bool mousePressed)
{
	if (mousePressed) {
		const glm::vec2 delta = mousePos - this->mousePos;
		const glm::quat deltaQuat = glm::quat(glm::vec3(mouseSpeed * delta.y, mouseSpeed * delta.x, 0.0f));
		cameraOrientation = glm::normalize(deltaQuat * cameraOrientation);
	}
	this->mousePos = mousePos;

	const glm::mat4 view = glm::mat4_cast(cameraOrientation);
	const glm::vec3 forward = -glm::vec3(view[0][2], view[1][2], view[2][2]);
	const glm::vec3 right = glm::vec3(view[0][0], view[1][0], view[2][0]);
	const glm::vec3 up = glm::cross(right, forward);

	glm::vec3 accel(0.0f);
	if (movement.forward) accel += forward;
	if (movement.backward) accel -= forward;
	if (movement.left) accel -= right;
	if (movement.right) accel += right;
	if (movement.up) accel += up;
	if (movement.down) accel -= up;
	if (movement.fastSpeed) accel *= fastCoef;

	if (accel == glm::vec3(0.0f)) {
		this->moveSpeed -= this->moveSpeed * std::fmin((1.0f / damping) * static_cast<float>(deltaSeconds), 1.0f);
	}
	else {
		this->moveSpeed += accel * acceleration * static_cast<float>(deltaSeconds);
		const float maxSpeed = movement.fastSpeed ? this->maxSpeed * fastCoef : this->maxSpeed;
		if (glm::length(this->moveSpeed) > maxSpeed) {
			this->moveSpeed = glm::normalize(this->moveSpeed) * maxSpeed;
		}
	}
	cameraPos += this->moveSpeed * static_cast<float>(deltaSeconds);
}

glm::mat4 CameraPositionerFirstPerson::getViewMatrix() const
{
	const glm::mat4 translation = glm::translate(glm::mat4(1.0f), -cameraPos);
	const glm::mat4 rotation = glm::mat4_cast(cameraOrientation);
	return rotation * translation;
}

void CameraPositionerFirstPerson::setUpVector(const glm::vec3& up)
{
	const glm::mat4 view = getViewMatrix();
	const glm::vec3 dir = -glm::vec3(view[0][2], view[1][2], view[2][2]);
	cameraOrientation = glm::lookAt(cameraPos, cameraPos + dir, up);
}
