#pragma once

#include <glm/glm.hpp>
#include <glm/ext.hpp>

class CameraPositionerInterface {
public:
	virtual ~CameraPositionerInterface() = default;
	virtual glm::mat4 getViewMatrix() const = 0;
	virtual glm::vec3 getPosition() const = 0;
};

class Camera final {
public:
	explicit Camera(CameraPositionerInterface& positioner) : positioner(positioner) {}
	glm::mat4 getViewMatrix() const { return positioner.getViewMatrix(); }
	glm::vec3 getPosition() const { return positioner.getPosition(); }
private:
	CameraPositionerInterface& positioner;
};

class CameraPositionerFirstPerson final : public CameraPositionerInterface {
public:
	CameraPositionerFirstPerson() = default;
	CameraPositionerFirstPerson(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up)
		: cameraPos(pos), cameraOrientation(glm::lookAt(pos, target, up)) {}

	virtual glm::mat4 getViewMatrix() const override;
	virtual glm::vec3 getPosition() const override { return cameraPos; }

	void update(double deltaSeconds, const glm::vec2& mousePos, bool mousePressed);
	void setPosition(const glm::vec3& pos) { cameraPos = pos; }
	void setUpVector(const glm::vec3& up);
	void resetMousePos(const glm::vec2& pos) { mousePos = pos; }

	struct Movement {
		bool forward = false;
		bool backward = false;
		bool left = false;
		bool right = false;
		bool up = false;
		bool down = false;
		bool fastSpeed = false;
	} movement;
	float mouseSpeed = 4.0f;
	float acceleration = 15.0f;
	float damping = 0.2f;
	float maxSpeed = 10.0f;
	float fastCoef = 10.0f;
private:
	glm::vec2 mousePos = glm::vec2(0.0f);
	glm::vec3 cameraPos = glm::vec3(0.0f, 10.0f, 10.0f);
	glm::quat cameraOrientation = glm::quat(glm::vec3(0));
	glm::vec3 moveSpeed = glm::vec3(0.0f);
};
