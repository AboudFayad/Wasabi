#include "Wasabi/Core/WOrientation.hpp"

WOrientation::WOrientation() {
	//default values
	m_pos = WVector3(0.0f, 0.0f, 0.0f);
	m_right = WVector3(1.0f, 0.0f, 0.0f);
	m_up = WVector3(0.0f, 1.0f, 0.0f);
	m_look = WVector3(0.0f, 0.0f, 1.0f);

	m_bBind = false;
}

WOrientation::~WOrientation() {
}

void WOrientation::SetPosition(float x, float y, float z) {
	m_pos = WVector3(x, y, z);

	OnStateChange(CHANGE_MOTION);
}

void WOrientation::SetPosition(const WVector3 pos) {
	m_pos = pos;

	OnStateChange(CHANGE_MOTION);
}

void WOrientation::Point(float x, float y, float z) {
	m_look = WVec3Normalize(WVector3(x, y, z) - m_pos);
	WVector3 temp = WVec3Cross(WVector3(0, 1, 0), m_look);
	m_right = WVec3Normalize(temp);
	m_up = WVec3Cross(m_look, m_right);

	OnStateChange(CHANGE_ROTATION);
}

void WOrientation::Point(WVector3 target) {
	m_look = WVec3Normalize(target - m_pos);
	WVector3 temp = WVec3Cross(WVector3(0, 1, 0), m_look);
	m_right = WVec3Normalize(temp);
	m_up = WVec3Cross(m_look, m_right);

	OnStateChange(CHANGE_ROTATION);
}

void WOrientation::SetAngle(WQuaternion quat) {
	WMatrix m1(quat.w, quat.z, -quat.y, quat.x,
		-quat.z, quat.w, quat.x, quat.y,
		quat.y, -quat.x, quat.w, quat.z,
		-quat.x, -quat.y, -quat.z, quat.w);

	WMatrix m2(quat.w, quat.z, -quat.y, -quat.x,
		-quat.z, quat.w, quat.x, -quat.y,
		quat.y, -quat.x, quat.w, -quat.z,
		quat.x, quat.y, quat.z, quat.w);

	WMatrix f = m1 * m2;

	//build the ulr vectors from the combined matrix
	m_up = WVector3(f(1, 0), f(1, 1), f(1, 2));
	m_look = WVector3(f(2, 0), f(2, 1), f(2, 2));
	m_right = WVector3(f(0, 0), f(0, 1), f(0, 2));

	OnStateChange(CHANGE_ROTATION);
}

void WOrientation::SetToRotation(const WOrientation* const device) {
	//copy input device's ulr vectors into object's ulr vectors
	m_look = device->GetLVector();
	m_up = device->GetUVector();
	m_right = device->GetRVector();

	OnStateChange(CHANGE_ROTATION);
}

void WOrientation::SetULRVectors(WVector3 up, WVector3 look, WVector3 right) {
	m_up = up;
	m_look = look;
	m_right = right;

	OnStateChange(CHANGE_ROTATION);
}

void WOrientation::SetToTransformation(WMatrix mtx) {
	m_pos = WVector3(mtx(0, 3), mtx(1, 3), mtx(2, 3));
	mtx = WMatrixInverse(mtx);
	m_right = WVector3(mtx(0, 0), mtx(1, 0), mtx(2, 0));
	m_up    = WVector3(mtx(0, 1), mtx(1, 1), mtx(2, 1));
	m_look  = WVector3(mtx(0, 2), mtx(1, 2), mtx(2, 2));

	OnStateChange(CHANGE_ROTATION | CHANGE_MOTION);
}

void WOrientation::Yaw(float angle) {
	//convert angle to radians
	angle = W_DEGTORAD(angle);
	WMatrix T = WRotationMatrixAxis(m_up, angle);

	// rotate m_right and m_look around m_up or y-axis
	m_right = WVec3TransformCoord(m_right, T);
	m_look = WVec3TransformCoord(m_look, T);

	OnStateChange(CHANGE_ROTATION);
}

void WOrientation::Roll(float angle) {
	//convert angle to radians
	angle = W_DEGTORAD(angle);
	WMatrix T = WRotationMatrixAxis(m_look, angle);

	// rotate m_up and m_right around m_look vector
	m_right = WVec3TransformCoord(m_right, T);
	m_up = WVec3TransformCoord(m_up, T);

	OnStateChange(CHANGE_ROTATION);
}

void WOrientation::Pitch(float angle) {
	//convert angle to radians
	angle = W_DEGTORAD(angle);
	WMatrix T = WRotationMatrixAxis(m_right, angle);

	// rotate m_up and m_look around m_right vector
	m_up = WVec3TransformCoord(m_up, T);
	m_look = WVec3TransformCoord(m_look, T);

	OnStateChange(CHANGE_ROTATION);
}

void WOrientation::Move(float units) {
	//increase on the forward axis to move forward
	m_pos += m_look * units;

	OnStateChange(CHANGE_MOTION);
}

void WOrientation::Strafe(float units) {
	//increase on the right axis to strafe
	m_pos += m_right * units;

	OnStateChange(CHANGE_MOTION);
}

void WOrientation::Fly(float units) {
	//increase on the forward axis to fly
	m_pos += m_up * units;

	OnStateChange(CHANGE_MOTION);
}

float WOrientation::GetPositionX() const {
	return m_pos.x;
}

float WOrientation::GetPositionY() const {
	return m_pos.y;
}

float WOrientation::GetPositionZ() const {
	return m_pos.z;
}

WVector3 WOrientation::GetPosition() const {
	return m_pos;
}

float WOrientation::GetAngleX() const {
	WVector3 zVec(0.0f, 0.0f, 1.0f); //origin axis
	WVector3 _2dLook(0.0f, m_look.y, m_look.z);

	float dot = WVec3Dot(zVec, _2dLook); //get angle between the vectors
	float u = 1.0f;
	float v = sqrt(float(_2dLook.x*_2dLook.x + _2dLook.y*_2dLook.y + _2dLook.z*_2dLook.z));

	if (v == 0)
		return 0;

	float result = acos(float(dot / (u*v)));
	if (m_look.y > 0.0f) //flip the angle if the object is in opposite direction
		result = -result;

	//return result in degrees
	return W_RADTODEG(result);
}

float WOrientation::GetAngleY() const {
	WVector3 yVec(0.0f, 0.0f, 1.0f); //origin axis
	WVector3 _2dLook(m_look.x, 0.0f, m_look.z);

	float dot = WVec3Dot(yVec, _2dLook); //get angle between the vectors
	float u = 1.0f;
	float v = sqrt(float(_2dLook.x*_2dLook.x + _2dLook.y*_2dLook.y + _2dLook.z*_2dLook.z));

	if (v == 0)
		return 0;

	float result = acos(float(dot / (u*v)));
	if (m_look.x < 0.0f) //flip the angle if the object is in opposite direction
		result = -result;

	//return result in degrees
	return W_RADTODEG(result);
}

float WOrientation::GetAngleZ() const {
	WVector3 yVec(0.0f, 1.0f, 0.0f); //origin axis
	WVector3 _2dLook(m_up.x, m_up.y, 0.0f);

	float dot = WVec3Dot(yVec, _2dLook); //get angle between the vectors
	float u = 1.0f;
	float v = sqrt(float(_2dLook.x*_2dLook.x + _2dLook.y*_2dLook.y + _2dLook.z*_2dLook.z));

	if (v == 0)
		return 0;

	float result = acos(float(dot / (u*v)));
	if (m_up.x > 0.0f) //flip the angle if the object is in opposite direction
		result = -result;

	//return result in degrees
	return W_RADTODEG(result);
}

WQuaternion WOrientation::GetRotation() const {
	WMatrix matrix = ComputeTransformation();
	float w = sqrt(1 + matrix(0, 0) + matrix(1, 1) + matrix(2, 2)) / 2;
	float x = (matrix(2, 1) - matrix(1, 2)) / (4 * w);
	float y = (matrix(0, 2) - matrix(2, 0)) / (4 * w);
	float z = (matrix(1, 0) - matrix(0, 1)) / (4 * w);
	return WQuaternion(x, y, z, w);
}

WVector3 WOrientation::GetUVector() const {
	return m_up;
}

WVector3 WOrientation::GetLVector() const {
	return m_look;
}

WVector3 WOrientation::GetRVector() const {
	return m_right;
}

void WOrientation::SetBindingMatrix(WMatrix mtx) {
	m_bBind = true;
	m_bindMtx = mtx;
}

void WOrientation::RemoveBinding() {
	m_bBind = false;
}

WMatrix WOrientation::GetBindingMatrix() const {
	return m_bindMtx;
}

bool WOrientation::IsBound() const {
	return m_bBind;
}

void WOrientation::OnStateChange(STATE_CHANGE_TYPE type) {
	if (type & CHANGE_ROTATION) {
		// Keep camera's axes orthogonal to eachother
		m_look = WVec3Normalize(m_look);

		m_up = WVec3Cross(m_look, m_right);
		m_up = WVec3Normalize(m_up);

		m_right = WVec3Cross(m_up, m_look);
		m_right = WVec3Normalize(m_right);
	}
}

WMatrix WOrientation::ComputeTransformation() const {
	WMatrix worldM = ComputeInverseTransformation();
	return WMatrixInverse(worldM);
}

WMatrix WOrientation::ComputeInverseTransformation() const {
	WMatrix worldM;
	WVector3 _up = GetUVector();
	WVector3 _look = GetLVector();
	WVector3 _right = GetRVector();
	WVector3 _pos = GetPosition();

	//
	//the world matrix is the view matrix's inverse
	//so we build a normal view matrix and invert it
	//

	//build world matrix
	float x = -WVec3Dot(_right, _pos);
	float y = -WVec3Dot(_up, _pos);
	float z = -WVec3Dot(_look, _pos);
	(worldM)(0, 0) = _right.x; (worldM)(0, 1) = _up.x; (worldM)(0, 2) = _look.x; (worldM)(0, 3) = 0.0f;
	(worldM)(1, 0) = _right.y; (worldM)(1, 1) = _up.y; (worldM)(1, 2) = _look.y; (worldM)(1, 3) = 0.0f;
	(worldM)(2, 0) = _right.z; (worldM)(2, 1) = _up.z; (worldM)(2, 2) = _look.z; (worldM)(2, 3) = 0.0f;
	(worldM)(3, 0) = x;        (worldM)(3, 1) = y;     (worldM)(3, 2) = z;       (worldM)(3, 3) = 1.0f;
	return worldM;
}
