#include "scene.h"
#include "../constants/constants.h"
#include "../assets/assetManager.h"
#include "../mesh/box.h"
#include "../mesh/sphere.h"
#include "../mesh/board.h"
#include "../mesh/quad.h"
#include "../mesh/model.h"
#include "../mesh/terrain.h"
#include "../object/staticObject.h"
using namespace std;

Scene::Scene() {
	time = 0.0, velocity = 0.0;
	inited = false;
	player = new Player();
	mainCamera = new Camera(25.0);
	reflectCamera = NULL;
	skyBox = NULL;
	water = NULL;
	terrainNode = NULL;
	textureNode = NULL;
	noise3d = NULL;
	
	staticRoot = NULL;
	billboardRoot = NULL;
	animationRoot = NULL;
	initNodes();
	boundingNodes.clear();
	meshCount.clear();
	meshes.clear();
	animCount.clear();
	anims.clear();
	animPlayers.clear();
	Node::nodesToUpdate.clear();
	Node::nodesToRemove.clear();
	Instance::instanceTable.clear();
}

Scene::~Scene() {
	delete player;
	if (mainCamera) delete mainCamera; mainCamera = NULL;
	if (reflectCamera) delete reflectCamera; reflectCamera = NULL;
	if (skyBox) delete skyBox; skyBox = NULL;
	if (water) delete water; water = NULL;
	if (terrainNode) delete terrainNode; terrainNode = NULL;
	if (textureNode) delete textureNode; textureNode = NULL;
	if (noise3d) delete noise3d; noise3d = NULL;
	if (staticRoot) delete staticRoot; staticRoot = NULL;
	if (billboardRoot) delete billboardRoot; billboardRoot = NULL;
	if (animationRoot) delete animationRoot; animationRoot = NULL;
	meshCount.clear();
	clearAllAABB();
	for (uint i = 0; i < meshes.size(); ++i)
		delete meshes[i];
	meshes.clear();
	anims.clear();
	animPlayers.clear();
	animCount.clear();
}

void Scene::initNodes() {
	staticRoot = new StaticNode(vec3(0, 0, 0));
	billboardRoot = new StaticNode(vec3(0, 0, 0));
	animationRoot = new StaticNode(vec3(0, 0, 0));
}

void Scene::updateNodes() {
	uint size = Node::nodesToUpdate.size();
	if (size == 0) return;
	for (uint i = 0; i < size; i++)
		Node::nodesToUpdate[i]->updateNode();
	Node::nodesToUpdate.clear();
}

void Scene::flushNodes() {
	uint size = Node::nodesToRemove.size();
	if (size == 0) return;
	for (uint i = 0; i < size; i++)
		delete Node::nodesToRemove[i];
	Node::nodesToRemove.clear();
}

void Scene::updateReflectCamera() {
	if (water && reflectCamera) {
		static mat4 transMat = scaleY(-1) * translate(0, water->position.y, 0);
		reflectCamera->viewMatrix = mainCamera->viewMatrix * transMat;
		reflectCamera->lookDir.x = mainCamera->lookDir.x;
		reflectCamera->lookDir.y = -mainCamera->lookDir.y;
		reflectCamera->lookDir.z = mainCamera->lookDir.z;
		reflectCamera->forceRefresh();
		reflectCamera->updateFrustum();
	}
}

void Scene::createReflectCamera() {
	if (reflectCamera) delete reflectCamera;
	reflectCamera = new Camera(0.0);
}

void Scene::createSky(bool dyn) {
	if (skyBox) delete skyBox;
	skyBox = new Sky(this, dyn);
}

void Scene::createWater(const vec3& position, const vec3& size) {
	if (water) delete water;
	water = new WaterNode(vec3(0, 0, 0));
	water->setFullStatic(true);
	StaticObject* waterObject = new StaticObject(AssetManager::assetManager->meshes["water"]);
	waterObject->setPosition(position.x, position.y, position.z);
	waterObject->setSize(size.x, size.y, size.z);
	water->addObject(this, waterObject);
	water->updateNode();
	water->prepareDrawcall();
}

void Scene::createTerrain(const vec3& position, const vec3& size) {
	if (terrainNode) delete terrainNode;
	terrainNode = new TerrainNode(position);
	terrainNode->setFullStatic(true);
	StaticObject* terrainObject = new StaticObject(AssetManager::assetManager->meshes["terrain"]);
	terrainObject->bindMaterial(MaterialManager::materials->find("terrain_mat"));
	terrainObject->setSize(size.x, size.y, size.z);
	terrainNode->addObject(this, terrainObject);
	terrainNode->prepareCollisionData();
	terrainNode->updateNode();
	terrainNode->prepareDrawcall();
	//staticRoot->attachChild(terrainNode);
}

void Scene::updateVisualTerrain(int bx, int bz, int sizex, int sizez) {
	if (!terrainNode) return;
	terrainNode->cauculateBlockIndices(bx, bz, sizex, sizez);
}

void Scene::createNodeAABB(Node* node) {
	AABB* aabb = (AABB*)node->boundingBox;
	if(aabb) {
		StaticNode* aabbNode = new StaticNode(aabb->position);
		StaticObject* aabbObject = new StaticObject(AssetManager::assetManager->meshes["box"]);
		aabbNode->setDynamicBatch(false);
		aabbObject->bindMaterial(MaterialManager::materials->find(BLACK_MAT));
		aabbObject->setSize(aabb->sizex, aabb->sizey, aabb->sizez);
		aabbNode->addObject(this, aabbObject);
		aabbNode->updateNode();
		aabbNode->prepareDrawcall();
		aabbNode->updateRenderData();
		aabbNode->updateDrawcall();
		boundingNodes.push_back(aabbNode);
	}
	for (uint i = 0; i < node->children.size(); i++)
		createNodeAABB(node->children[i]);
	
	if (node->children.size() == 0) {
		for (uint i = 0; i < node->objects.size(); i++) {
			AABB* aabb = (AABB*)node->objects[i]->bounding;
			if (aabb) {
				StaticNode* aabbNode = new StaticNode(aabb->position);
				StaticObject* aabbObject = new StaticObject(AssetManager::assetManager->meshes.find("box")->second);
				aabbNode->setDynamicBatch(false);
				aabbObject->bindMaterial(MaterialManager::materials->find(BLACK_MAT));
				aabbObject->setSize(aabb->sizex, aabb->sizey, aabb->sizez);
				aabbNode->addObject(this, aabbObject);
				aabbNode->updateNode();
				aabbNode->prepareDrawcall();
				aabbNode->updateRenderData();
				aabbNode->updateDrawcall();
				boundingNodes.push_back(aabbNode);
			}
		}
	}
}

void Scene::clearAllAABB() {
	for (uint i = 0; i<boundingNodes.size(); i++)
		delete boundingNodes[i];
	boundingNodes.clear();
}

void Scene::addObject(Object* object) {
	Mesh* cur = object->mesh;
	if (cur) {
		if (meshCount.find(cur) == meshCount.end()) {
			meshCount[cur] = 0;
			meshes.push_back(new MeshObject(cur, object));
		}
		meshCount[cur]++;
	}
	cur = object->meshMid;
	if (cur && cur != object->mesh) {
		if (meshCount.find(cur) == meshCount.end()) {
			meshCount[cur] = 0;
			meshes.push_back(new MeshObject(cur, object));
		}
		meshCount[cur]++;
	}
	cur = object->meshLow;
	if (cur && cur != object->meshMid && cur != object->mesh) {
		if (meshCount.find(cur) == meshCount.end()) {
			meshCount[cur] = 0;
			meshes.push_back(new MeshObject(cur, object));
		}
		meshCount[cur]++;
	}
	// Animation object
	if (!object->mesh) {
		AnimationObject* animObj = (AnimationObject*)object;
		if (animObj) {
			Animation* curAnim = animObj->animation;
			if (animCount.find(curAnim) == animCount.end()) {
				animCount[curAnim] = 0;
				anims.push_back(curAnim);
			}
			animCount[curAnim]++;
		}
	}
}

void Scene::addPlay(AnimationNode* node) {
	animPlayers.push_back(node);
}

uint Scene::queryMeshCount(Mesh* mesh) {
	if (meshCount.find(mesh) == meshCount.end())
		meshCount[mesh] = 0;
	return meshCount[mesh];
}

