#include <iostream>
#include <cstdio>
#include <vector>
#include <thread>
#include <algorithm>
#include "Ogre.h"
#include "OgreApplicationContext.h"
#include "OgreInput.h"
#include "OgrePass.h"
#include "OgreRTShaderSystem.h"
#include "OgrePixelFormat.h"
#include "d3d9types.h"


using namespace Ogre;
using namespace OgreBites;

const double eps = 1e-2;
const double unit = 50;

class RayTracing
	: public ApplicationContext
	, public InputListener
{	

	struct MeshInfo {
		size_t vertex_count;
		size_t index_count;
		Ogre::Vector3 *vertices;
		Ogre::uint32 *indices;
		AxisAlignedBox AABBbox;
		MeshInfo(Entity * entity) {
			AABBbox = entity->getWorldBoundingBox(true);
			GetMeshInformation(entity->getMesh().getPointer(), vertex_count, vertices, index_count, indices,
				entity->getParentNode()->_getDerivedPosition(),
				entity->getParentNode()->_getDerivedOrientation(),
				entity->getParentNode()->_getDerivedScale());

		}
	};
	
	SceneManager* scnMgr;
	SceneNode* camNode;
	std::map<Entity*, MeshInfo*> meshInfoMap;
	std::vector<Entity*> entityList;
	std::vector<Light*> lightList;
	
	const bool enableAA = true;
	const int tracingDepthLimit = 3, renderReslutionRatio = 1;
	const int height = 200, length = 300, width = 240;

	// Get the mesh information for the given mesh.
	static void GetMeshInformation(const Mesh* mesh, size_t &vertex_count, Vector3* &vertices,
		size_t &index_count, unsigned* &indices,
		const Vector3 &position = Vector3::ZERO,
		const Quaternion &orient = Quaternion::IDENTITY, const Vector3 &scale = Vector3::UNIT_SCALE)
	{
		vertex_count = index_count = 0;

		bool added_shared = false;
		size_t current_offset = vertex_count;
		size_t shared_offset = vertex_count;
		size_t next_offset = vertex_count;
		size_t index_offset = index_count;
		size_t prev_vert = vertex_count;
		size_t prev_ind = index_count;

		// Calculate how many vertices and indices we're going to need
		for (int i = 0; i < mesh->getNumSubMeshes(); i++)
		{
			SubMesh* submesh = mesh->getSubMesh(i);

			// We only need to add the shared vertices once
			if (submesh->useSharedVertices)
			{
				if (!added_shared)
				{
					VertexData* vertex_data = mesh->sharedVertexData;
					vertex_count += vertex_data->vertexCount;
					added_shared = true;
				}
			}
			else
			{
				VertexData* vertex_data = submesh->vertexData;
				vertex_count += vertex_data->vertexCount;
			}

			// Add the indices
			Ogre::IndexData* index_data = submesh->indexData;
			index_count += index_data->indexCount;
		}

		// Allocate space for the vertices and indices
		vertices = new Vector3[vertex_count];
		indices = new unsigned[index_count];

		added_shared = false;

		// Run through the submeshes again, adding the data into the arrays
		for (int i = 0; i < mesh->getNumSubMeshes(); i++)
		{
			SubMesh* submesh = mesh->getSubMesh(i);

			Ogre::VertexData* vertex_data = submesh->useSharedVertices ? mesh->sharedVertexData : submesh->vertexData;
			if ((!submesh->useSharedVertices) || (submesh->useSharedVertices && !added_shared))
			{
				if (submesh->useSharedVertices)
				{
					added_shared = true;
					shared_offset = current_offset;
				}

				const Ogre::VertexElement* posElem = vertex_data->vertexDeclaration->findElementBySemantic(Ogre::VES_POSITION);
				Ogre::HardwareVertexBufferSharedPtr vbuf = vertex_data->vertexBufferBinding->getBuffer(posElem->getSource());
				unsigned char* vertex = static_cast<unsigned char*>(vbuf->lock(Ogre::HardwareBuffer::HBL_READ_ONLY));
				Ogre::Real* pReal;

				for (size_t j = 0; j < vertex_data->vertexCount; ++j, vertex += vbuf->getVertexSize())
				{
					posElem->baseVertexPointerToElement(vertex, &pReal);

					Vector3 pt;

					pt.x = (*pReal++);
					pt.y = (*pReal++);
					pt.z = (*pReal++);

					pt = (orient * (pt * scale)) + position;

					vertices[current_offset + j].x = pt.x;
					vertices[current_offset + j].y = pt.y;
					vertices[current_offset + j].z = pt.z;
				}
				vbuf->unlock();
				next_offset += vertex_data->vertexCount;
			}

			Ogre::IndexData* index_data = submesh->indexData;

			size_t numTris = index_data->indexCount / 3;
			unsigned short* pShort;
			unsigned int* pInt;
			Ogre::HardwareIndexBufferSharedPtr ibuf = index_data->indexBuffer;
			bool use32bitindexes = (ibuf->getType() == Ogre::HardwareIndexBuffer::IT_32BIT);
			if (use32bitindexes) pInt = static_cast<unsigned int*>(ibuf->lock(Ogre::HardwareBuffer::HBL_READ_ONLY));
			else pShort = static_cast<unsigned short*>(ibuf->lock(Ogre::HardwareBuffer::HBL_READ_ONLY));

			for (size_t k = 0; k < numTris; ++k)
			{
				size_t offset = (submesh->useSharedVertices) ? shared_offset : current_offset;

				unsigned int vindex = use32bitindexes ? *pInt++ : *pShort++;
				indices[index_offset + 0] = vindex + offset;
				vindex = use32bitindexes ? *pInt++ : *pShort++;
				indices[index_offset + 1] = vindex + offset;
				vindex = use32bitindexes ? *pInt++ : *pShort++;
				indices[index_offset + 2] = vindex + offset;

				index_offset += 3;
			}
			ibuf->unlock();
			current_offset = next_offset;
		}
	}

	static void SaveImage(TexturePtr TextureToSave, String filename)
	{
		HardwarePixelBufferSharedPtr readbuffer;
		readbuffer = TextureToSave->getBuffer(0, 0);
		readbuffer->lock(HardwareBuffer::HBL_NORMAL);
		const PixelBox &readrefpb = readbuffer->getCurrentLock();
		uchar *readrefdata = static_cast<uchar*>(readrefpb.data);

		Image img;
		img = img.loadDynamicImage(readrefdata, TextureToSave->getWidth(),
			TextureToSave->getHeight(), TextureToSave->getFormat());
		img.save(filename);

		readbuffer->unlock();
	}

	void traceRow(Camera *viewCamera, uint32 *data, int width, int height, int j) {
		for (size_t i = 0; i < width; i += 1) {
			Ray ray;
			ray = viewCamera->getCameraToViewportRay((1.0 * i) / width, (1.0 * j) / height);
			ColourValue color1 = Tracer(ray, 1);
			if (!enableAA) {
				data[i] = D3DCOLOR_COLORVALUE(color1.r, color1.g, color1.b, 1.0);
				continue;
			}
			ray = viewCamera->getCameraToViewportRay((1.0 * i + 0.5) / width, (1.0 * j) / height);
			ColourValue color2 = Tracer(ray, 1);
			ray = viewCamera->getCameraToViewportRay((1.0 * i) / width, (1.0 * j + 0.5) / height);
			ColourValue color3 = Tracer(ray, 1);
			ray = viewCamera->getCameraToViewportRay((1.0 * i + 0.5) / width, (1.0 * j + 0.5) / height);
			ColourValue color4 = Tracer(ray, 1);
			ColourValue color = (color1 + color2 + color3 + color4) / 4;
			/*if (color.r > 1 || color.g > 1 || color.b > 1) {
				printf("(%d, %d): ", j, i);
				std::cout << color << "\n";
			}*/
			data[i] = D3DCOLOR_COLORVALUE(color.r, color.g, color.b, 1.0);

				
		}
	}

	static ColourValue calcColor(ColourValue I, ColourValue ani, ColourValue diff, ColourValue spec, double sin, Vector3 N, Vector3 L, Vector3 V) {
		Vector3 H = V + L;
		H.normalise();
		L.normalise();
		V.normalise();
		N.normalise();
		ColourValue diffuse = diff * I * std::max(Ogre::Real(0), N.dotProduct(L));
		ColourValue specular = spec * I * pow(std::max(Ogre::Real(0), N.dotProduct(H)), sin);
		return diffuse + specular;
	}

	bool build_ray() {
		Camera* viewCamera = scnMgr->getCamera("myCam");
		size_t width = viewCamera->getViewport()->getActualWidth() * renderReslutionRatio;
		size_t height = viewCamera->getViewport()->getActualHeight() * renderReslutionRatio;

		Ogre::TexturePtr texPtr = Ogre::TextureManager::getSingleton().getByName("MyTraceTex");
		if (texPtr.isNull()) {
			texPtr = Ogre::TextureManager::getSingleton().createManual(
				"MyTraceTex",
				"General",
				Ogre::TEX_TYPE_2D,
				width, height,
				0, Ogre::PF_A8R8G8B8);
			if (texPtr.isNull())
				return false;
		}

		texPtr->getBuffer(0, 0)->lock(Ogre::HardwareBuffer::HBL_DISCARD);
		const Ogre::PixelBox &pb = texPtr->getBuffer(0, 0)->getCurrentLock();

		uint32 *data = reinterpret_cast<uint32*>(pb.data);

		height = pb.getHeight();
		width = pb.getWidth();
		size_t pitch = pb.rowPitch;
		std::queue<std::thread*> threadQueue;
		for (size_t j = 0; j < height; j += 1){
			printf("tracing row %d\n", j);
			if (threadQueue.size() == 10) {
				std::thread *head = threadQueue.front();
				threadQueue.pop();
				head->join();
				delete head;
			}
			threadQueue.push(new std::thread(std::mem_fn(&RayTracing::traceRow), this, viewCamera, data + pitch * j, width, height, j));
			//traceRow(viewCamera, data + pitch * j, width, height, j);
		}
		while (!threadQueue.empty()) {
			std::thread *head = threadQueue.front();
			threadQueue.pop();
			head->join();
			delete head;
		}

		texPtr->getBuffer(0, 0)->unlock();

		SaveImage(texPtr, "../../RarTracing.png");

		return true;
	}

	RayTestResult InterectionTest(Ray ray, Entity *entity,  Plane* &p, bool needP = true) {
		MeshInfo* info = meshInfoMap[entity];
		if (ray.intersects(info->AABBbox).first == false) {
			return std::pair<bool, Real>(false, 0);
		}
		size_t vertex_count = info->vertex_count;
		size_t index_count = info->index_count;
		Ogre::Vector3 *vertices = info->vertices;
		Ogre::uint32 *indices = info->indices;
		RayTestResult result;
		result.first = false;
		result.second = 1e100;
		p = nullptr;
		for (size_t i = 0; i < index_count; i += 3){
			Vector3 a = vertices[indices[i]];
			Vector3 b = vertices[indices[i + 1]];
			Vector3 c = vertices[indices[i + 2]];
			RayTestResult hit = Ogre::Math::intersects(ray, a, b, c, true, false);
			if (hit.first && (result.first == false || result.second > hit.second)){
				result = hit;
				p = needP ? new Plane(a, b, c) : nullptr;
			}
		}
		
		return result;
	}

	std::pair<ColourValue, Vector3> getLightInfo(Light *light, Vector3 p) {
		Vector3 lightPosition = light->getDerivedPosition();
		Vector3 dir = p - lightPosition;
		double length = dir.length();
		dir.normalise();
		Ray ray(lightPosition, dir);
		Plane* tmpPlane;
		for (int i = 0; i < entityList.size(); ++i) {
			Entity *now = entityList[i];
			RayTestResult InterectionResult = InterectionTest(ray, now, tmpPlane, false);
			if (InterectionResult.first && InterectionResult.second < length - eps) {
				return std::pair<ColourValue, Vector3>(ColourValue::ZERO, Vector3::ZERO);
			}
		}
		length = std::max(length / unit, 1.);
		return std::pair<ColourValue, Vector3>(light->getDiffuseColour() * light->getPowerScale() / (length * length), dir);
	}

	ColourValue Tracer(Ray ray, int depth){
		
		RayTestResult result(false, 1e100);
		Plane* refPlane = nullptr;
		Entity *refEntity;
		for (int i = 0; i < entityList.size(); ++i){
			Entity *now = entityList[i];
			Plane* tmpPlane;
			RayTestResult IntersectResult = InterectionTest(ray, now, tmpPlane);
			if (IntersectResult.first) {
				if (!result.first || (result.first && result.second > IntersectResult.second)) {
					result = IntersectResult;
					refEntity = now;
					if (refPlane != nullptr)
						delete refPlane;
					refPlane = tmpPlane;
				}
				else
					delete tmpPlane;
			}
		}
		
		ColourValue color = ColourValue::ZERO;
		Ray Reflect;
		if (result.first == true) {
			Material *material = refEntity->getSubEntity(0)->getMaterial().getPointer();
			Pass *pass = material->getTechnique(0)->getPass(0);

			Vector3 p = ray.getPoint(result.second);
			Vector3 rayDir = ray.getDirection();
			Vector3 V = ray.getOrigin() - p;
			Vector3 N = refPlane->normal;
			if (N.dotProduct(rayDir) > 0)
				N = -N;

			N.normalise();
			V.normalise();
			Vector3 rayRelDir = rayDir.reflect(N);
			Reflect.setOrigin(p);
			Reflect.setDirection(rayRelDir);

			ColourValue ani = pass->getAmbient();
			ColourValue diff = pass->getDiffuse();
			ColourValue spec = pass->getSpecular();
			double sin = pass->getShininess();
			
			if (depth > 1)
				diff = ColourValue::ZERO;
			for (int j = 0; j < lightList.size(); ++j)
			{
				std::pair<ColourValue, Vector3> light = getLightInfo(lightList[j], p);
				ColourValue I = light.first; //light 
				if (I == ColourValue::ZERO)
					continue;
				color += calcColor(I, ani, diff, spec, sin, N, -light.second, V);
			}
			if (tracingDepthLimit > depth) {
				ColourValue I = Tracer(Reflect, depth + 1);
				color += calcColor(I, ani, diff, spec, sin, N, rayRelDir, V);
			}
			if (depth == 1) {
				color += scnMgr->getAmbientLight() * pass->getAmbient();
			}
			else{
				// ray reduce
				color = color / pow(std::max((p - ray.getOrigin()).length() / unit, 1.), 2);
			}
			delete refPlane;
		}
		
		return color;
	}


public:
	RayTracing(): ApplicationContext("Ray Tracing App") {}
	virtual ~RayTracing() {}


	void setup()
	{
		
		// do not forget to call the base first
		ApplicationContext::setup();
		addInputListener(this);
		
		// get a pointer to the already created root
		Root* root = getRoot();
		scnMgr = root->createSceneManager();

		// register our scene with the RTSS
		RTShader::ShaderGenerator* shadergen = RTShader::ShaderGenerator::getSingletonPtr();
		shadergen->addSceneManager(scnMgr);

		//! [turnlights]
		scnMgr->setAmbientLight(ColourValue(0.2, 0.2, 0.2));

		//! [newlight]
		Light* light = scnMgr->createLight("MainLight");
		light->setType(Light::LightTypes::LT_POINT);
		light->setPowerScale(20);
		SceneNode* lightNode = scnMgr->getRootSceneNode()->createChildSceneNode();
		lightNode->attachObject(light);
		lightNode->setPosition(0, 50, 100);
		lightList.push_back(light);

		// create camera and add 
		camNode = scnMgr->getRootSceneNode()->createChildSceneNode();
		Camera* cam = scnMgr->createCamera("myCam");
		cam->setNearClipDistance(60); // specific to this sample
		cam->setAutoAspectRatio(true);
		camNode->attachObject(cam);
		camNode->setPosition(0, 0, 200);
		getRenderWindow()->addViewport(cam);
		//! [camera]

		// build objects
		{
			//! [entity1]
			Entity* ogreEntity1 = scnMgr->createEntity("robot.mesh");
			SceneNode* ogreNode1 = scnMgr->getRootSceneNode()->createChildSceneNode("Object1");
			ogreEntity1->setMaterialName("Examples/myObject1");
			ogreNode1->attachObject(ogreEntity1);
			//ogreNode->scale(0.5, 0.5, 0.5);
			ogreNode1->yaw(Radian(-3.1415926 / 2));
			ogreNode1->setPosition(0, -50, -100);
			entityList.push_back(ogreEntity1);

			//! [entity2]
			Entity* ogreEntity2 = scnMgr->createEntity("cube.mesh");
			SceneNode* ogreNode2 = scnMgr->getRootSceneNode()->createChildSceneNode("Object2");
			ogreEntity2->setMaterialName("Examples/myObject2");
			ogreNode2->attachObject(ogreEntity2);
			ogreNode2->scale(0.4, 0.4, 0.4);
			//ogreNode2->yaw(Radian(-3.1415926 / 2));
			ogreNode2->setPosition(60, -20, -100);
			entityList.push_back(ogreEntity2);

			//! [entity3]
			Entity* ogreEntity3 = scnMgr->createEntity("geosphere4500.mesh");
			SceneNode* ogreNode3 = scnMgr->getRootSceneNode()->createChildSceneNode("Object3");
			ogreEntity3->setMaterialName("Examples/myObject2");
			ogreNode3->attachObject(ogreEntity3);
			ogreNode3->scale(0.125, 0.125, 0.125);
			//ogreNode3->yaw(Radian(-3.1415926 / 2));
			ogreNode3->setPosition(-60, -20, -100);
			entityList.push_back(ogreEntity3);
		}

		// build thr room box
		{
			Plane p;
			p.normal = Vector3::UNIT_Y;
			p.d = height / 2;
			MeshManager::getSingleton().createPlane("FloorPlane",
				ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
				p, width, length, 1, 1, true, 1, 5, 5, Vector3::UNIT_Z);
			Entity* floor = scnMgr->createEntity("floor", "FloorPlane");
			floor->setMaterialName("Examples/myPlane");
			scnMgr->getRootSceneNode()->attachObject(floor);
			entityList.push_back(floor);

			p.normal = -Vector3::UNIT_Y;
			p.d = height / 2;
			MeshManager::getSingleton().createPlane("RoofPlane",
				ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
				p, width, length, 1, 1, true, 1, 5, 5, -Vector3::UNIT_Z);
			Entity* roof = scnMgr->createEntity("roof", "RoofPlane");
			roof->setMaterialName("Examples/myPlane");
			scnMgr->getRootSceneNode()->attachObject(roof);
			entityList.push_back(roof);


			p.normal = Vector3::UNIT_Z;
			p.d = length / 2;
			MeshManager::getSingleton().createPlane("WallPlaneF",
				ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
				p, width, height, 1, 1, true, 1, 5, 5, Vector3::UNIT_Y);
			Entity* wallF = scnMgr->createEntity("wallF", "WallPlaneF");
			wallF->setMaterialName("Examples/myWall");
			scnMgr->getRootSceneNode()->attachObject(wallF);
			entityList.push_back(wallF);

			p.normal = -Vector3::UNIT_Z;
			p.d = length / 2;
			MeshManager::getSingleton().createPlane("WallPlaneB",
				ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
				p, width, height, 1, 1, true, 1, 5, 5, -Vector3::UNIT_Y);
			Entity* wallB = scnMgr->createEntity("wallB", "WallPlaneB");
			wallB->setMaterialName("Examples/myWall");
			scnMgr->getRootSceneNode()->attachObject(wallB);
			entityList.push_back(wallB);

			p.normal = Vector3::UNIT_X;
			p.d = width / 2;
			MeshManager::getSingleton().createPlane("WallPlaneL",
				ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
				p, length, height, 1, 1, true, 1, 5, 5, Vector3::UNIT_Y);
			Entity* wallL = scnMgr->createEntity("wallL", "WallPlaneL");
			wallL->setMaterialName("Examples/myWall");
			scnMgr->getRootSceneNode()->attachObject(wallL);
			entityList.push_back(wallL);

			p.normal = -Vector3::UNIT_X;
			p.d = width / 2;
			MeshManager::getSingleton().createPlane("WallPlaneR",
				ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
				p, length, height, 1, 1, true, 1, 5, 5, -Vector3::UNIT_Y);
			Entity* wallR = scnMgr->createEntity("wallR", "WallPlaneR");
			wallR->setMaterialName("Examples/myWall");
			scnMgr->getRootSceneNode()->attachObject(wallR);
			entityList.push_back(wallR);
		}

		for (int i = 0; i < entityList.size(); i++) {
			meshInfoMap[entityList[i]] = new MeshInfo(entityList[i]);
		}
		std::cout << build_ray() << std::endl;
	}


	bool keyPressed(const KeyboardEvent& evt)
	{
		switch (evt.keysym.sym) {
		case SDLK_ESCAPE:
			getRoot()->queueEndRendering();
			break;

		case 'w':
			camNode->translate(0, 5, 0);
			break;

		case 'a':
			camNode->translate(-5, 0, 0);
			break;

		case 's':
			camNode->translate(0, -5, 0);
			break;

		case 'd':
			camNode->translate(5, 0, 0);
			break;

		case 'i':
			camNode->translate(0, 0, 5);
			break;

		case 'o':
			camNode->translate(0, 0, -5);
			break;

		case 'u':
			camNode->pitch(Radian(0.1));
			break;

		case 'n':
			camNode->pitch(Radian(-0.1));
			break;

		case 'l':
			camNode->yaw(Radian(0.1));
			break;

		case 'r':
			camNode->yaw(Radian(-0.1));
			break;

		//case 'z':
		//	objectNode->roll(Radian(0.05));
		//	break;

		//case 'k':
		//	objectNode->roll(Radian(-0.05));
		//	break;

		}
		return true;
	}

};


int main(int argc, char **argv){
	try{
		RayTracing app;
		app.initApp();
		app.getRoot()->startRendering();
		app.closeApp();
	}
	catch (const std::exception& e){
		std::cerr << "Error occurred during execution: " << e.what() << '\n';
		return 1;
	}

	return 0;
}