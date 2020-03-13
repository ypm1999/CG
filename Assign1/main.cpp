#include "Ogre.h"
#include "OgreApplicationContext.h"
#include "OgreInput.h"
#include "OgreRTShaderSystem.h"
#include <iostream>

using namespace Ogre;
using namespace OgreBites;

class BasicTutorial1
	: public ApplicationContext
	, public InputListener
{
public:
	BasicTutorial1();
	virtual ~BasicTutorial1() {}

	void setup();
	bool keyPressed(const KeyboardEvent& evt);
};


BasicTutorial1::BasicTutorial1()
	: ApplicationContext("OgreTutorialApp")
{
}


void BasicTutorial1::setup()
{
	// do not forget to call the base first
	ApplicationContext::setup();
	addInputListener(this);

	// get a pointer to the already created root
	Root* root = getRoot();
	SceneManager* scnMgr = root->createSceneManager();

	// register our scene with the RTSS
	RTShader::ShaderGenerator* shadergen = RTShader::ShaderGenerator::getSingletonPtr();
	shadergen->addSceneManager(scnMgr);

	//! [turnlights]
	scnMgr->setAmbientLight(ColourValue(0.5, 0.5, 0.5));

	//! [newlight]
	Light* light = scnMgr->createLight("MainLight");
	SceneNode* lightNode = scnMgr->getRootSceneNode()->createChildSceneNode();
	lightNode->attachObject(light);
	

	//! [lightpos]
	lightNode->setPosition(20, 80, 50);

	//! [camera]
	SceneNode* camNode = scnMgr->getRootSceneNode()->createChildSceneNode();

	// create the camera
	Camera* cam = scnMgr->createCamera("myCam");
	cam->setNearClipDistance(5); // specific to this sample
	cam->setAutoAspectRatio(true);
	camNode->attachObject(cam);
	camNode->setPosition(0, 0, 140);

	// and tell it to render into the main window
	getRenderWindow()->addViewport(cam);
	//! [camera]

	//! [entity1]
	Entity* ogreEntity = scnMgr->createEntity("ogrehead.mesh");
	SceneNode* ogreNode = scnMgr->getRootSceneNode()->createChildSceneNode("Object");
	ogreNode->attachObject(ogreEntity);
	camNode->setPosition(0, 0, 140);
}


bool BasicTutorial1::keyPressed(const KeyboardEvent& evt)
{
	Root* root = getRoot();
	SceneNode* sceNode = (root->getSceneManagers().begin()->second)->getRootSceneNode();
	SceneNode* objectNode = (SceneNode*)(sceNode->getChild("Object"));
	/*std::cout << (objectNode->getAttachedObjects()).size() << " ";
	std::cout << int(evt.keysym.sym) << std::endl;*/
	switch (evt.keysym.sym) {
	case SDLK_ESCAPE:
		getRoot()->queueEndRendering();
		break;

	case 'w':
		objectNode->translate(0, 1, 0);
		break;

	case 'a':
		objectNode->translate(-1, 0, 0);
		break;

	case 's':
		objectNode->translate(0, -1, 0);
		break;

	case 'd':
		objectNode->translate(1, 0, 0);
		break;

	case 'x': 
		objectNode->pitch(Radian(0.05));
		break;

	case 'i':
		objectNode->pitch(Radian(-0.05));
		break;

	case 'y': 
		objectNode->yaw(Radian(0.05));
		break;

	case 'j':
		objectNode->yaw(Radian(-0.05));
		break;

	case 'z': 
		objectNode->roll(Radian(0.05));
		break; 

	case 'k':
		objectNode->roll(Radian(-0.05));
		break;

	case 'l':
		objectNode->scale(1.05, 1.05, 1.05);
		break;

	case 'm':
		objectNode->scale(0.95, 0.95, 0.95);
		break;
	}
	return true;
}


int main(int argc, char **argv)
{
	try
	{
		BasicTutorial1 app;
		app.initApp();
		app.getRoot()->startRendering();
		app.closeApp();
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error occurred during execution: " << e.what() << '\n';
		return 1;
	}

	return 0;
}