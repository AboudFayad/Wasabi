#include "Wasabi/Physics/Bullet/WBulletRigidBody.hpp"
#include "Wasabi/Physics/Bullet/WBulletPhysics.hpp"
#include "Wasabi/Physics/WRigidBody.hpp"
#include "Wasabi/Core/WCore.hpp"
#include "Wasabi/Geometries/WGeometry.hpp"

#include <btBulletCollisionCommon.h>
#include <btBulletDynamicsCommon.h>

WBulletRigidBodyManager::WBulletRigidBodyManager(class Wasabi* const app)
	: WRigidBodyManager(app) {
}

WBulletRigidBodyManager::~WBulletRigidBodyManager() {
}

std::string WBulletRigidBodyManager::GetTypeName() const {
	return "RigidBodyManager";
}

void WBulletRigidBodyManager::Update(float deltaTime) {
	for (uint32_t i = 0; i < W_HASHTABLESIZE; i++) {
		for (uint32_t j = 0; j < m_entities[i].size(); j++) {
			((WBulletRigidBody*)m_entities[i][j])->Update(deltaTime);
		}
	}
}

WBulletRigidBody::WBulletRigidBody(Wasabi* const app, uint32_t ID) : WRigidBody(app, ID) {
	m_isUpdating = false;
	m_boundObject = nullptr;
	m_boundObjectBase = nullptr;
	m_savedCreateInfo = nullptr;

	m_collisionShape = nullptr;
	m_rigidBody = nullptr;

	app->PhysicsComponent->RigidBodyManager->AddEntity(this);
}

WBulletRigidBody::~WBulletRigidBody() {
	BindObject(nullptr);
	_DestroyResources();

	m_app->PhysicsComponent->RigidBodyManager->RemoveEntity(this);
}

std::string WBulletRigidBody::_GetTypeName() {
	return "RigidBody";
}

std::string WBulletRigidBody::GetTypeName() const {
	return _GetTypeName();
}

void WBulletRigidBody::SetID(uint32_t newID) {
	m_app->PhysicsComponent->RigidBodyManager->RemoveEntity(this);
	m_ID = newID;
	m_app->PhysicsComponent->RigidBodyManager->AddEntity(this);
}

void WBulletRigidBody::SetName(std::string newName) {
	m_name = newName;
	m_app->PhysicsComponent->RigidBodyManager->OnEntityNameChanged(this, newName);
}

void WBulletRigidBody::_DestroyResources() {
	WBulletPhysics* physics = (WBulletPhysics*)m_app->PhysicsComponent;

	if (m_rigidBody) {
		if (((btRigidBody*)m_rigidBody)->getMotionState())
			delete ((btRigidBody*)m_rigidBody)->getMotionState();
		((btDynamicsWorld*)physics->m_dynamicsWorld)->removeCollisionObject(((btRigidBody*)m_rigidBody));
	}

	btRigidBody* rigidBody = (btRigidBody*)m_rigidBody;
	btCollisionShape* collisionShape = (btCollisionShape*)m_collisionShape;
	W_SAFE_DELETE(rigidBody);
	W_SAFE_DELETE(collisionShape);
	if (m_savedCreateInfo) {
		W_SAFE_REMOVEREF(m_savedCreateInfo->geometry);
		W_SAFE_DELETE(m_savedCreateInfo);
	}
}

bool WBulletRigidBody::Valid() const {
	return m_rigidBody;
}

WError WBulletRigidBody::Create(W_RIGID_BODY_CREATE_INFO createInfo, bool bSaveInfo) {
	if (createInfo.mass < 0.0f ||
		((createInfo.shape == RIGID_BODY_SHAPE_CONVEX || createInfo.shape == RIGID_BODY_SHAPE_MESH) && !createInfo.geometry) ||
		(createInfo.shape == RIGID_BODY_SHAPE_MESH && createInfo.geometry->GetNumIndices() <= 0 && createInfo.isTriangleList))
		return WError(W_INVALIDPARAM);

	_DestroyResources();

	WBulletPhysics* physics = (WBulletPhysics*)m_app->PhysicsComponent;

	switch (createInfo.shape) {
	case RIGID_BODY_SHAPE_CUBE:
		m_collisionShape = (void*)new btBoxShape(WBTConvertVec3(createInfo.dimensions));
		break;
	case RIGID_BODY_SHAPE_SPHERE:
		m_collisionShape = (void*)new btSphereShape(btScalar(createInfo.dimensions.x));
		break;
	case RIGID_BODY_SHAPE_CAPSULE:
		m_collisionShape = (void*)new btCapsuleShape(btScalar(createInfo.dimensions.y), btScalar(createInfo.dimensions.x));
		break;
	case RIGID_BODY_SHAPE_CYLINDER:
		m_collisionShape = (void*)new btCylinderShape(WBTConvertVec3(createInfo.dimensions / WVector3(1.0f, 2.0f, 1.0f)));
		break;
	case RIGID_BODY_SHAPE_CONE:
		m_collisionShape = (void*) new btConeShape(btScalar(createInfo.dimensions.y), btScalar(createInfo.dimensions.x));
		break;
	case RIGID_BODY_SHAPE_CONVEX:
	{
		W_VERTEX_DESCRIPTION vertexDesc = createInfo.geometry->GetVertexDescription();
		size_t stride = vertexDesc.GetSize();
		uint32_t numVerts = createInfo.geometry->GetNumVertices();
		size_t posOffset = vertexDesc.GetOffset(W_ATTRIBUTE_POSITION.name);
		if (posOffset == std::numeric_limits<size_t>::max())
			return WError(W_INVALIDPARAM);
		float* vb = nullptr;
		createInfo.geometry->MapVertexBuffer((void**)&vb, W_MAP_READ);
		btScalar* points = new btScalar[numVerts*3];
		for (uint32_t i = 0; i < numVerts; i++) {
			points[i*3 + 0] = (btScalar)*(float*)((char*)vb + stride * i + posOffset + 0);
			points[i*3 + 1] = (btScalar)*(float*)((char*)vb + stride * i + posOffset + 4);
			points[i*3 + 2] = (btScalar)*(float*)((char*)vb + stride * i + posOffset + 8);
		}
		createInfo.geometry->UnmapVertexBuffer();
		m_collisionShape = (void*)new btConvexHullShape(points, numVerts, 3 * sizeof(btScalar));
		delete[] points;
		break;
	}
	case RIGID_BODY_SHAPE_MESH:
	{
		W_VERTEX_DESCRIPTION vertexDesc = createInfo.geometry->GetVertexDescription();
		size_t stride = vertexDesc.GetSize();
		size_t posOffset = vertexDesc.GetOffset(W_ATTRIBUTE_POSITION.name);
		if (posOffset == std::numeric_limits<size_t>::max())
			return WError(W_INVALIDPARAM);
		btScalar* points = nullptr;
		uint32_t* indices = nullptr;
		createInfo.geometry->MapVertexBuffer((void**)&points, W_MAP_READ);
		if (createInfo.isTriangleList)
			createInfo.geometry->MapIndexBuffer((void**)&indices, W_MAP_READ);

		uint32_t num_triangles = createInfo.isTriangleList ? createInfo.geometry->GetNumIndices() / 3 : createInfo.geometry->GetNumVertices() - 2;
		btTriangleMesh* mesh = new btTriangleMesh(true, false);
		for (uint32_t tri = 0; tri < num_triangles; tri++) {
			int i0 = tri, i1 = tri + 1, i2 = tri + 2;
			if (createInfo.isTriangleList) {
				i0 = indices[tri * 3 + 0], i1 = indices[tri * 3 + 1], i2 = indices[tri * 3 + 2];
			}
			WVector3 t0 = *(WVector3*)((char*)points + (i0 * stride) + posOffset);
			WVector3 t1 = *(WVector3*)((char*)points + (i1 * stride) + posOffset);
			WVector3 t2 = *(WVector3*)((char*)points + (i2 * stride) + posOffset);
			mesh->addTriangle(WBTConvertVec3(t0), WBTConvertVec3(t1), WBTConvertVec3(t2), false);
		}
		createInfo.mass = 0.0f;
		m_collisionShape = (void*)new btBvhTriangleMeshShape(mesh, true);
		createInfo.geometry->UnmapVertexBuffer();
		if (createInfo.isTriangleList)
			createInfo.geometry->UnmapIndexBuffer();
		break;
	}
	}

	btTransform transformation;
	transformation.setIdentity();
	if (createInfo.orientation) {
		transformation = WBTConverMatrix(createInfo.orientation->ComputeTransformation());
		createInfo.initialPosition = createInfo.orientation->GetPosition();
		createInfo.initialRotation = createInfo.orientation->GetRotation();
	} else {
		transformation.setOrigin(WBTConvertVec3(createInfo.initialPosition));
		transformation.setRotation(WBTConvertQuaternion(createInfo.initialRotation));
	}

	btScalar mass(createInfo.mass);

	//rigidbody is dynamic if and only if mass is non zero, otherwise static
	btVector3 localInertia(0, 0, 0);
	if (mass > 0.0001f)
		((btCollisionShape*)m_collisionShape)->calculateLocalInertia(mass, localInertia);

	//using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
	btDefaultMotionState* motionState = new btDefaultMotionState(transformation);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, ((btCollisionShape*)m_collisionShape), localInertia);
	m_rigidBody = (void*)new btRigidBody(rbInfo);

	//add the body to the dynamics world
	((btDynamicsWorld*)physics->m_dynamicsWorld)->addRigidBody(((btRigidBody*)m_rigidBody));

	SetBouncingPower(0.2f);
	SetFriction(0.2f);

	if (bSaveInfo) {
		m_savedCreateInfo = new W_RIGID_BODY_CREATE_INFO(createInfo);
		m_savedCreateInfo->orientation = nullptr;
		if (m_savedCreateInfo->geometry)
			m_savedCreateInfo->geometry->AddReference(); // hold a reference to the geometry
	}

	return WError(W_SUCCEEDED);
}

void WBulletRigidBody::Update(float deltaTime) {
	UNREFERENCED_PARAMETER(deltaTime);

	m_isUpdating = true;
	if (m_rigidBody) {
		btQuaternion rotation = ((btRigidBody*)m_rigidBody)->getOrientation();
		WQuaternion wRot = BTWConvertQuaternion(rotation);
		btTransform transformation = ((btRigidBody*)m_rigidBody)->getWorldTransform();
		btVector3 position = transformation.getOrigin();
		WVector3 wPos = BTWConvertVec3(position);
		SetPosition(wPos);
		SetAngle(wRot);

		if (m_boundObject) {
			m_boundObject->SetPosition(wPos);
			m_boundObject->SetAngle(wRot);
		}
	}
	m_isUpdating = false;
}

void WBulletRigidBody::BindObject(WOrientation* obj, WBase* objBase) {
	if (m_boundObjectBase)
		m_boundObjectBase->RemoveReference();

	m_boundObject = obj;
	m_boundObjectBase = objBase;

	if (m_boundObjectBase)
		m_boundObjectBase->AddReference();
}

void WBulletRigidBody::SetLinearVelocity(WVector3 vel) {
	if (m_rigidBody) {
		((btRigidBody*)m_rigidBody)->setLinearVelocity(WBTConvertVec3(vel));
		((btRigidBody*)m_rigidBody)->activate();
	}
}

void WBulletRigidBody::SetAngularVelocity(WVector3 vel) {
	if (m_rigidBody) {
		((btRigidBody*)m_rigidBody)->setAngularVelocity(WBTConvertVec3(vel));
		((btRigidBody*)m_rigidBody)->activate();
	}
}

void WBulletRigidBody::SetLinearDamping(float power) {
	if (m_rigidBody)
		((btRigidBody*)m_rigidBody)->setDamping(btScalar(power), ((btRigidBody*)m_rigidBody)->getAngularDamping());
}

void WBulletRigidBody::SetAngularDamping(float power) {
	if (m_rigidBody)
		((btRigidBody*)m_rigidBody)->setDamping(((btRigidBody*)m_rigidBody)->getLinearDamping(), btScalar(power));
}

void WBulletRigidBody::SetBouncingPower(float bouncing) {
	if (m_rigidBody)
		((btRigidBody*)m_rigidBody)->setRestitution(btScalar(bouncing));
}

void WBulletRigidBody::SetMass(float mass) {
	if (m_rigidBody)
		((btRigidBody*)m_rigidBody)->setMassProps(btScalar(mass), ((btRigidBody*)m_rigidBody)->getLocalInertia());
}

void WBulletRigidBody::SetMassCenter(float x, float y, float z) {
	if (m_rigidBody)
		((btRigidBody*)m_rigidBody)->setCenterOfMassTransform(WBTConverMatrix(WTranslationMatrix(x, y, z)));
}

void WBulletRigidBody::SetFriction(float friction) {
	if (m_rigidBody)
		((btRigidBody*)m_rigidBody)->setFriction(btScalar(friction));
}

void WBulletRigidBody::ApplyForce(WVector3 force) {
	if (m_rigidBody) {
		((btRigidBody*)m_rigidBody)->applyCentralForce(WBTConvertVec3(force));
		((btRigidBody*)m_rigidBody)->activate();
	}
}

void WBulletRigidBody::ApplyForce(WVector3 force, WVector3 relative_pos) {
	if (m_rigidBody) {
		((btRigidBody*)m_rigidBody)->applyForce(WBTConvertVec3(force), WBTConvertVec3(relative_pos));
		((btRigidBody*)m_rigidBody)->activate();
	}
}

void WBulletRigidBody::ApplyImpulse(WVector3 impulse) {
	if (m_rigidBody) {
		((btRigidBody*)m_rigidBody)->applyCentralImpulse(WBTConvertVec3(impulse));
		((btRigidBody*)m_rigidBody)->activate();
	}
}

void WBulletRigidBody::ApplyImpulse(WVector3 impulse, WVector3 relative_pos) {
	if (m_rigidBody) {
		((btRigidBody*)m_rigidBody)->applyImpulse(WBTConvertVec3(impulse), WBTConvertVec3(relative_pos));
		((btRigidBody*)m_rigidBody)->activate();
	}
}

void WBulletRigidBody::ApplyTorque(WVector3 torque) {
	if (m_rigidBody) {
		((btRigidBody*)m_rigidBody)->applyTorque(WBTConvertVec3(torque));
		((btRigidBody*)m_rigidBody)->activate();
	}
}

WVector3 WBulletRigidBody::getLinearVelocity() const {
	if (m_rigidBody)
		return BTWConvertVec3(((btRigidBody*)m_rigidBody)->getLinearVelocity());
	return WVector3();
}

WVector3 WBulletRigidBody::getAngularVelocity() const {
	if (m_rigidBody)
		return BTWConvertVec3(((btRigidBody*)m_rigidBody)->getAngularVelocity());
	return WVector3();
}

WVector3 WBulletRigidBody::getTotalForce() const {
	if (m_rigidBody)
		return BTWConvertVec3(((btRigidBody*)m_rigidBody)->getTotalForce());
	return WVector3();
}

WVector3 WBulletRigidBody::getTotalTorque() const {
	if (m_rigidBody)
		return BTWConvertVec3(((btRigidBody*)m_rigidBody)->getTotalTorque());
	return WVector3();
}

WMatrix WBulletRigidBody::GetWorldMatrix() {
	return ComputeTransformation();
}

void WBulletRigidBody::OnStateChange(STATE_CHANGE_TYPE type) {
	UNREFERENCED_PARAMETER(type);

	if (!m_isUpdating && m_rigidBody) {
		btTransform mtx = WBTConverMatrix(GetWorldMatrix());
		((btRigidBody*)m_rigidBody)->setWorldTransform(mtx);
		((btRigidBody*)m_rigidBody)->getMotionState()->setWorldTransform(mtx);
		((btRigidBody*)m_rigidBody)->activate();
	}
}

WError WBulletRigidBody::SaveToStream(class WFile* file, std::ostream& outputStream) {
	if (!Valid() || !m_savedCreateInfo)
		return WError(W_NOTVALID);

	outputStream.write((char*)m_savedCreateInfo, sizeof(W_RIGID_BODY_CREATE_INFO));
	char geometryName[W_MAX_ASSET_NAME_SIZE] = { '\0' };
	if (m_savedCreateInfo->geometry)
		strcpy_s(geometryName, W_MAX_ASSET_NAME_SIZE, m_savedCreateInfo->geometry->GetName().c_str());
	outputStream.write(geometryName, W_MAX_ASSET_NAME_SIZE);

	float linDamp = ((btRigidBody*)m_rigidBody)->getLinearDamping();
	outputStream.write((char*)&linDamp, sizeof(linDamp));
	float angDamp = ((btRigidBody*)m_rigidBody)->getAngularDamping();
	outputStream.write((char*)&angDamp, sizeof(angDamp));
	float bouncePower = ((btRigidBody*)m_rigidBody)->getRestitution();
	outputStream.write((char*)&bouncePower, sizeof(bouncePower));
	float mass = ((btRigidBody*)m_rigidBody)->getInvMass();
	outputStream.write((char*)&mass, sizeof(mass));
	WVector3 centerOfMass = BTWConvertVec3(((btRigidBody*)m_rigidBody)->getCenterOfMassPosition());
	outputStream.write((char*)&centerOfMass, sizeof(centerOfMass));
	float friction = ((btRigidBody*)m_rigidBody)->getFriction();
	outputStream.write((char*)&friction, sizeof(friction));

	WError err(W_SUCCEEDED);
	if (m_savedCreateInfo->geometry)
		file->SaveAsset(m_savedCreateInfo->geometry);
	return err;
}

std::vector<void*> WBulletRigidBody::LoadArgs(bool bSaveInfo) {
	return std::vector<void*>({
		(void*)bSaveInfo,
	});
}

WError WBulletRigidBody::LoadFromStream(class WFile* file, std::istream& inputStream, std::vector<void*>& args, std::string nameSuffix) {
	if (args.size() != 1)
		return WError(W_INVALIDPARAM);
	bool bSaveInfo = (bool)args[0];

	_DestroyResources();

	W_RIGID_BODY_CREATE_INFO info = { 0 };
	inputStream.read((char*)&info, sizeof(W_RIGID_BODY_CREATE_INFO));
	info.orientation = nullptr;
	char geometryName[W_MAX_ASSET_NAME_SIZE];
	inputStream.read(geometryName, W_MAX_ASSET_NAME_SIZE);

	float linDamp, angDamp, bouncePower, mass, friction;
	WVector3 centerOfMass;
	inputStream.read((char*)&linDamp, sizeof(linDamp));
	inputStream.read((char*)&angDamp, sizeof(angDamp));
	inputStream.read((char*)&bouncePower, sizeof(bouncePower));
	inputStream.read((char*)&mass, sizeof(mass));
	inputStream.read((char*)&centerOfMass, sizeof(centerOfMass));
	inputStream.read((char*)&friction, sizeof(friction));
	SetLinearDamping(linDamp);
	SetAngularDamping(angDamp);
	SetBouncingPower(bouncePower);
	SetMass(mass);
	SetMassCenter(centerOfMass.x, centerOfMass.y, centerOfMass.z);
	SetFriction(friction);

	if (strlen(geometryName) > 0) {
		WError err = file->LoadAsset<WGeometry>(geometryName, &info.geometry, WGeometry::LoadArgs(), ""); // never copy geometry, always share
		if (!err)
			return err;
	}

	Create(info, bSaveInfo);
	W_SAFE_REMOVEREF(info.geometry);

	return WError(W_SUCCEEDED);
}
